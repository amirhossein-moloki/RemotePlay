#include "encoder_hw.hpp"
#include <iostream>

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
        std::cerr << "[Encoder] No hardware H.264 encoder found (nvenc, amf, qsv)." << std::endl;
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
        std::cerr << "[Encoder] Failed to open codec." << std::endl;
        return false;
    }

    m_internal->pkt = av_packet_alloc();
    m_internal->frame = av_frame_alloc();

    return true;
}

bool FFmpegHardwareEncoder::EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets) {
    if (!m_internal->codecCtx || !texturePtr) return false;

    // Zero-copy: Pass the texture directly to FFmpeg.
    // In a real environment, the AVFrame data pointer would be set to the ID3D11Texture2D.
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

        EncodedPacket p;
        p.data.assign(m_internal->pkt->data, m_internal->pkt->data + m_internal->pkt->size);
        p.isKeyframe = (m_internal->pkt->flags & AV_PKT_FLAG_KEY);
        p.timestamp = m_internal->pkt->pts;
        outPackets.push_back(std::move(p));

        av_packet_unref(m_internal->pkt);
    }

    return true;
}

void FFmpegHardwareEncoder::Shutdown() {
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
bool FFmpegHardwareEncoder::EncodeFrame(void* t, std::vector<EncodedPacket>& o) { return false; }
void FFmpegHardwareEncoder::Shutdown() {}
#endif

} // namespace Host
