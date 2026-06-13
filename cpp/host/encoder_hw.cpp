#include "encoder_hw.hpp"
#include <iostream>
#include "../common/logger.hpp"

#ifdef _WIN32
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}
#endif

namespace Host {

#ifdef _WIN32
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

bool FFmpegHardwareEncoder::Initialize(int width, int height, int fps, int bitrateKbps) {
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
    av_opt_set(m_internal->codecCtx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(m_internal->codecCtx->priv_data, "rc", "cbr", 0);

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

    // Use a local timestamp to avoid any thread-safety issues with frameCounter if called from multiple threads
    // though the current architecture is single-threaded per encoder.
    m_internal->frame->data[0] = (uint8_t*)texturePtr;
    m_internal->frame->format = AV_PIX_FMT_D3D11;
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
                ep.timestamp = (uint64_t)m_internal->pkt->pts;

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
    if (m_internal->codecCtx) avcodec_free_context(&m_internal->codecCtx);
    if (m_internal->hwDeviceCtx) av_buffer_unref(&m_internal->hwDeviceCtx);
    if (m_internal->frame) av_frame_free(&m_internal->frame);
    if (m_internal->pkt) av_packet_free(&m_internal->pkt);
}

#else
struct FFmpegHardwareEncoder::InternalData {};
FFmpegHardwareEncoder::FFmpegHardwareEncoder() : m_internal(nullptr) {}
FFmpegHardwareEncoder::~FFmpegHardwareEncoder() {}
bool FFmpegHardwareEncoder::Initialize(int w, int h, int f, int b) { return false; }
bool FFmpegHardwareEncoder::EncodeFrame(void* t, std::vector<EncodedPacket>& o, PacketPool& p) { return false; }
void FFmpegHardwareEncoder::SetBitrate(int b) {}
void FFmpegHardwareEncoder::Shutdown() {}
#endif

} // namespace Host
