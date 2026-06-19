#include "decoder_hw.hpp"
#include <iostream>
#include "../common/logger.hpp"

#ifdef PARSEC_LITE_ENABLE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#endif

namespace Client {

#ifdef PARSEC_LITE_ENABLE_FFMPEG
struct DecoderHW::InternalData {
    AVCodecContext* codecCtx = nullptr;
    AVBufferRef* hwDeviceCtx = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;

    // Software fallback resources
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11Texture2D* swTexture = nullptr;
    SwsContext* swsCtx = nullptr;
    AVFrame* rgbaFrame = nullptr;
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

    if (d3d11DevicePtr) {
        m_internal->device = (ID3D11Device*)d3d11DevicePtr;
        m_internal->device->GetImmediateContext(&m_internal->context);
    }

    LOG_INFO("Decoder", "FFmpeg Decoder initialized.");
    return true;
}

bool DecoderHW::DecodeFrame(const uint8_t* data, size_t size, void** outTexture) {
    if (!m_internal->codecCtx) return false;

    av_packet_unref(m_internal->pkt);
    m_internal->pkt->data = (uint8_t*)data;
    m_internal->pkt->size = (int)size;

    int ret = avcodec_send_packet(m_internal->codecCtx, m_internal->pkt);
    if (ret < 0) {
        if (ret != AVERROR(EAGAIN)) {
            LOG_WARN("Decoder", "avcodec_send_packet failed (code: " + std::to_string(ret) + ")");
        }
        return false;
    }

    ret = avcodec_receive_frame(m_internal->codecCtx, m_internal->frame);
    if (ret < 0) {
        if (ret != AVERROR(EAGAIN)) {
            LOG_WARN("Decoder", "Decoding error (code: " + std::to_string(ret) + "). Possible missing SPS/PPS headers; awaiting next keyframe.");
        }
        return false;
    }

    if (m_internal->frame->format == AV_PIX_FMT_D3D11) {
        *outTexture = m_internal->frame->data[0]; // ID3D11Texture2D*
    } else {
        // Software frame fallback: Convert to RGBA and upload to D3D11 texture
        if (m_internal->device) {
            if (!m_internal->swsCtx || m_internal->rgbaFrame->width != m_internal->frame->width || m_internal->rgbaFrame->height != m_internal->frame->height) {
                if (m_internal->swsCtx) sws_freeContext(m_internal->swsCtx);
                if (m_internal->rgbaFrame) av_frame_free(&m_internal->rgbaFrame);
                if (m_internal->swTexture) { m_internal->swTexture->Release(); m_internal->swTexture = nullptr; }

                m_internal->swsCtx = sws_getContext(m_internal->frame->width, m_internal->frame->height, (AVPixelFormat)m_internal->frame->format,
                                                   m_internal->frame->width, m_internal->frame->height, AV_PIX_FMT_RGBA,
                                                   SWS_FAST_BILINEAR, NULL, NULL, NULL);

                m_internal->rgbaFrame = av_frame_alloc();
                m_internal->rgbaFrame->format = AV_PIX_FMT_RGBA;
                m_internal->rgbaFrame->width = m_internal->frame->width;
                m_internal->rgbaFrame->height = m_internal->frame->height;
                av_frame_get_buffer(m_internal->rgbaFrame, 0);

                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = m_internal->frame->width;
                desc.Height = m_internal->frame->height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                m_internal->device->CreateTexture2D(&desc, nullptr, &m_internal->swTexture);
            }

            sws_scale(m_internal->swsCtx, m_internal->frame->data, m_internal->frame->linesize, 0, m_internal->frame->height,
                      m_internal->rgbaFrame->data, m_internal->rgbaFrame->linesize);

            m_internal->context->UpdateSubresource(m_internal->swTexture, 0, nullptr, m_internal->rgbaFrame->data[0], m_internal->rgbaFrame->linesize[0], 0);
            *outTexture = m_internal->swTexture;
        }
    }

    return true;
}

bool DecoderHW::IsHardware() const {
    return m_internal && m_internal->codecCtx && m_internal->codecCtx->hw_device_ctx != nullptr;
}

void DecoderHW::Shutdown() {
    if (m_internal->swsCtx) sws_freeContext(m_internal->swsCtx);
    if (m_internal->rgbaFrame) av_frame_free(&m_internal->rgbaFrame);
    if (m_internal->swTexture) m_internal->swTexture->Release();
    if (m_internal->context) m_internal->context->Release();
    m_internal->swsCtx = nullptr;
    m_internal->rgbaFrame = nullptr;
    m_internal->swTexture = nullptr;
    m_internal->context = nullptr;

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
struct DecoderHW::InternalData {};
DecoderHW::DecoderHW() : m_internal(nullptr) {}
DecoderHW::~DecoderHW() {}
bool DecoderHW::Initialize(void* p) {
    LOG_ERROR("Decoder", "FFmpeg support not compiled in. Decoder is disabled.");
    return false;
}
bool DecoderHW::DecodeFrame(const uint8_t* d, size_t s, void** o) { return false; }
bool DecoderHW::IsHardware() const { return false; }
void DecoderHW::Shutdown() {}
#endif

} // namespace Client
