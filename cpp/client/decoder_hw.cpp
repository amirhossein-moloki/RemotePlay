#include "decoder_hw.hpp"
#include <iostream>
#include "../common/logger.hpp"

#ifdef _WIN32
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}
#endif

namespace Client {

#ifdef _WIN32
struct DecoderHW::InternalData {
    AVCodecContext* codecCtx = nullptr;
    AVBufferRef* hwDeviceCtx = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;
};

DecoderHW::DecoderHW() : m_internal(new InternalData()) {}
DecoderHW::~DecoderHW() {
    Shutdown();
    delete m_internal;
}

bool DecoderHW::Initialize(void* d3d11DevicePtr) {
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) return false;

    m_internal->codecCtx = avcodec_alloc_context3(codec);
    if (!m_internal->codecCtx) return false;

    if (d3d11DevicePtr) {
        m_internal->hwDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (m_internal->hwDeviceCtx) {
            AVHWDeviceContext* deviceCtx = (AVHWDeviceContext*)m_internal->hwDeviceCtx->data;
            AVD3D11VADeviceContext* d3d11Ctx = (AVD3D11VADeviceContext*)deviceCtx->hwctx;
            d3d11Ctx->device = (ID3D11Device*)d3d11DevicePtr;
            d3d11Ctx->device->AddRef();

            if (av_hwdevice_ctx_init(m_internal->hwDeviceCtx) < 0) {
                av_buffer_unref(&m_internal->hwDeviceCtx);
            } else {
                m_internal->codecCtx->hw_device_ctx = av_buffer_ref(m_internal->hwDeviceCtx);
            }
        }
    }

    if (avcodec_open2(m_internal->codecCtx, codec, NULL) < 0) {
        return false;
    }

    m_internal->pkt = av_packet_alloc();
    m_internal->frame = av_frame_alloc();

    LOG_INFO("Decoder", "FFmpeg Hardware Decoder initialized (D3D11VA).");
    return true;
}

bool DecoderHW::DecodeFrame(const uint8_t* data, size_t size, void** outTexture) {
    av_packet_unref(m_internal->pkt);
    m_internal->pkt->data = (uint8_t*)data;
    m_internal->pkt->size = (int)size;

    int ret = avcodec_send_packet(m_internal->codecCtx, m_internal->pkt);
    if (ret < 0) return false;

    ret = avcodec_receive_frame(m_internal->codecCtx, m_internal->frame);
    if (ret < 0) return false;

    if (m_internal->frame->format == AV_PIX_FMT_D3D11) {
        *outTexture = m_internal->frame->data[0]; // ID3D11Texture2D*
    }

    return true;
}

void DecoderHW::Shutdown() {
    if (m_internal->codecCtx) avcodec_free_context(&m_internal->codecCtx);
    if (m_internal->hwDeviceCtx) av_buffer_unref(&m_internal->hwDeviceCtx);
    if (m_internal->frame) av_frame_free(&m_internal->frame);
    if (m_internal->pkt) av_packet_free(&m_internal->pkt);
}
#else
struct DecoderHW::InternalData {};
DecoderHW::DecoderHW() : m_internal(nullptr) {}
DecoderHW::~DecoderHW() {}
bool DecoderHW::Initialize(void* p) { return false; }
bool DecoderHW::DecodeFrame(const uint8_t* d, size_t s, void** o) { return false; }
void DecoderHW::Shutdown() {}
#endif

} // namespace Client
