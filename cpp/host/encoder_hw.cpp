#include "encoder_hw.hpp"
#include <iostream>
#include "../common/logger.hpp"

#ifdef PARSEC_LITE_ENABLE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}
#endif

namespace Host {

#ifdef PARSEC_LITE_ENABLE_FFMPEG
struct FFmpegHardwareEncoder::InternalData {
    AVCodecContext* codecCtx = nullptr;
    AVBufferRef* hwDeviceCtx = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;
    int64_t frameCounter = 0;
};

FFmpegHardwareEncoder::FFmpegHardwareEncoder() : m_internal(new InternalData()) {}

FFmpegHardwareEncoder::~FFmpegHardwareEncoder() {
    Shutdown();
    delete m_internal;
}

bool FFmpegHardwareEncoder::Initialize(int width, int height, int fps, int bitrateKbps, void* d3d11Device) {
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_bitrate = bitrateKbps;

    const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) codec = avcodec_find_encoder_by_name("h264_amf");
    if (!codec) codec = avcodec_find_encoder_by_name("h264_qsv");
    if (!codec) {
        LOG_ERROR("Encoder", "No hardware H.264 encoder found (nvenc, amf, qsv).");
        return false;
    }

    m_internal->codecCtx = avcodec_alloc_context3(codec);
    if (!m_internal->codecCtx) return false;

    m_internal->codecCtx->width = width;
    m_internal->codecCtx->height = height;
    m_internal->codecCtx->time_base = { 1, fps };
    m_internal->codecCtx->framerate = { fps, 1 };
    m_internal->codecCtx->pix_fmt = AV_PIX_FMT_NV12;
    m_internal->codecCtx->bit_rate = bitrateKbps * 1000;
    m_internal->codecCtx->gop_size = 60;
    m_internal->codecCtx->max_b_frames = 0;

    av_opt_set(m_internal->codecCtx->priv_data, "preset", "p1", 0);
    if (std::string(m_internal->codecCtx->codec->name).find("nvenc") != std::string::npos) {
        av_opt_set(m_internal->codecCtx->priv_data, "tune", "ull", 0);
    } else {
        av_opt_set(m_internal->codecCtx->priv_data, "tune", "zerolatency", 0);
    }
    av_opt_set(m_internal->codecCtx->priv_data, "rc", "cbr", 0);

    if (d3d11Device) {
        AVBufferRef* device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (device_ref) {
            AVHWDeviceContext* device_ctx = (AVHWDeviceContext*)device_ref->data;
            AVD3D11VADeviceContext* d3d11_ctx = (AVD3D11VADeviceContext*)device_ctx->hwctx;
            d3d11_ctx->device = (ID3D11Device*)d3d11Device;
            // No AddRef here if we assume the device lifetime is managed by the caller
            // OR if FFmpeg's internal logic handles it.
            // Actually, FFmpeg doesn't AddRef it, so we should, BUT we must ensure it's released.
            // AVD3D11VADeviceContext doesn't automatically Release it on destruction if set manually.
            // Better to use FFmpeg's own device creation if possible, or manage it carefully.
            d3d11_ctx->device->AddRef();

            if (av_hwdevice_ctx_init(device_ref) >= 0) {
                m_internal->hwDeviceCtx = device_ref;
                m_internal->codecCtx->hw_device_ctx = av_buffer_ref(device_ref);

                // Initialize HW Frames Context for zero-copy
                AVBufferRef* frames_ref = av_hwframe_ctx_alloc(device_ref);
                AVHWFramesContext* frames_ctx = (AVHWFramesContext*)frames_ref->data;
                frames_ctx->format = AV_PIX_FMT_D3D11;
                frames_ctx->sw_format = AV_PIX_FMT_NV12;
                frames_ctx->width = width;
                frames_ctx->height = height;
                frames_ctx->initial_pool_size = 0; // Use 0 for wrapped textures

                if (av_hwframe_ctx_init(frames_ref) >= 0) {
                    m_internal->codecCtx->hw_frames_ctx = av_buffer_ref(frames_ref);
                    m_internal->codecCtx->pix_fmt = AV_PIX_FMT_D3D11;
                } else {
                    LOG_ERROR("Encoder", "Failed to initialize HW frames context.");
                }
                av_buffer_unref(&frames_ref);
            } else {
                LOG_ERROR("Encoder", "Failed to initialize HW device context.");
                av_buffer_unref(&device_ref);
            }
        }
    }

    if (avcodec_open2(m_internal->codecCtx, codec, NULL) < 0) {
        LOG_ERROR("Encoder", "Failed to open codec.");
        return false;
    }

    m_internal->pkt = av_packet_alloc();
    m_internal->frame = av_frame_alloc();

    m_initialized = true;
    return true;
}

bool FFmpegHardwareEncoder::EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets, PacketPool& pool) {
    if (!m_internal->codecCtx || !texturePtr) return false;

    av_frame_unref(m_internal->frame);

    if (m_internal->codecCtx->hw_frames_ctx) {
        // Zero-copy path: Wrap the D3D11 texture directly
        m_internal->frame->data[0] = (uint8_t*)texturePtr;
        m_internal->frame->format = AV_PIX_FMT_D3D11;
    } else {
        LOG_ERROR("Encoder", "Non-HW path not supported for D3D11 textures. Ensure HW Context is initialized.");
        return false;
    }

    m_internal->frame->width = m_internal->codecCtx->width;
    m_internal->frame->height = m_internal->codecCtx->height;
    m_internal->frame->pts = m_internal->frameCounter++;

    int ret = avcodec_send_frame(m_internal->codecCtx, m_internal->frame);
    if (ret < 0) return false;

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_internal->codecCtx, m_internal->pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) return false;

        auto pktBuffer = pool.acquire();
        if (pktBuffer) {
            if (m_internal->pkt->size <= pktBuffer->data.size()) {
                memcpy(pktBuffer->data.data(), m_internal->pkt->data, m_internal->pkt->size);
                pktBuffer->size = m_internal->pkt->size;

                EncodedPacket ep;
                ep.packet = std::move(pktBuffer);
                ep.isKeyframe = (m_internal->pkt->flags & AV_PKT_FLAG_KEY);
                // The timestamps will be populated by the caller who knows the real-world time
                ep.captureTimestamp = 0;
                ep.encodeStartTimestamp = 0;
                ep.encodeEndTimestamp = 0;

                // Reserve space to avoid reallocations if the vector is being reused
                if (outPackets.capacity() < outPackets.size() + 1) {
                    outPackets.reserve(std::max((size_t)8, outPackets.capacity() * 2));
                }
                outPackets.push_back(std::move(ep));
            } else {
                // In production, we might want to log this once or handle it by expanding the pool
                pool.release(std::move(pktBuffer));
            }
        }

        av_packet_unref(m_internal->pkt);
    }

    return true;
}

void FFmpegHardwareEncoder::SetBitrate(int bitrateKbps) {
    if (m_internal->codecCtx && m_bitrate != bitrateKbps) {
        m_bitrate = bitrateKbps;
        m_internal->codecCtx->bit_rate = (int64_t)bitrateKbps * 1000;
        m_internal->codecCtx->rc_max_rate = (int64_t)bitrateKbps * 1000;
        m_internal->codecCtx->rc_buffer_size = (int)((int64_t)bitrateKbps * 1000 / m_fps);
    }
}

void FFmpegHardwareEncoder::Shutdown() {
    m_initialized = false;
    if (m_internal->codecCtx) {
        if (m_internal->codecCtx->hw_device_ctx) {
            AVHWDeviceContext* device_ctx = (AVHWDeviceContext*)m_internal->codecCtx->hw_device_ctx->data;
            if (device_ctx->type == AV_HWDEVICE_TYPE_D3D11VA) {
                AVD3D11VADeviceContext* d3d11_ctx = (AVD3D11VADeviceContext*)device_ctx->hwctx;
                if (d3d11_ctx->device) d3d11_ctx->device->Release();
            }
        }
        avcodec_free_context(&m_internal->codecCtx);
    }
    if (m_internal->hwDeviceCtx) av_buffer_unref(&m_internal->hwDeviceCtx);
    if (m_internal->frame) av_frame_free(&m_internal->frame);
    if (m_internal->pkt) av_packet_free(&m_internal->pkt);
}

#else
struct FFmpegHardwareEncoder::InternalData {};
FFmpegHardwareEncoder::FFmpegHardwareEncoder() : m_internal(nullptr) {}
FFmpegHardwareEncoder::~FFmpegHardwareEncoder() {}
bool FFmpegHardwareEncoder::Initialize(int w, int h, int f, int b, void* d) {
    LOG_ERROR("Encoder", "FFmpeg support not compiled in. Hardware encoding is disabled.");
    return false;
}
bool FFmpegHardwareEncoder::EncodeFrame(void* t, std::vector<EncodedPacket>& o, PacketPool& p) { return false; }
void FFmpegHardwareEncoder::SetBitrate(int b) {}
void FFmpegHardwareEncoder::Shutdown() {}
#endif

} // namespace Host
