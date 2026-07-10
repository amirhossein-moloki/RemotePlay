#include "decoder_hw.hpp"
#include <iostream>
#include "../common/logger.hpp"

#ifdef PARSEC_LITE_ENABLE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#ifdef _WIN32
#include <libavutil/hwcontext_d3d11va.h>
#endif
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}
#endif
#include <cstdint>

namespace Client {

#ifdef PARSEC_LITE_ENABLE_FFMPEG

static enum AVPixelFormat get_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == AV_PIX_FMT_D3D11) {
            return *p;
        }
    }
    return AV_PIX_FMT_NONE;
}

struct DecoderHW::InternalData {
    AVCodecContext* codecCtx = nullptr;
    AVBufferRef* hwDeviceCtx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* tempFrame = nullptr;
    AVPacket* pkt = nullptr;
    bool isHEVC = false;
    void* lastD3D11Device = nullptr;
    bool lastUseHardware = true;

    // Software fallback resources
#ifdef _WIN32
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11Texture2D* swTexture = nullptr;
#endif
    SwsContext* swsCtx = nullptr;
    AVFrame* rgbaFrame = nullptr;
};

DecoderHW::DecoderHW() : m_internal(new InternalData()) {}
DecoderHW::~DecoderHW() {
    Shutdown();
    delete m_internal;
}

bool DecoderHW::Initialize(void* d3d11DevicePtr, bool useHardware) {
    Shutdown(); // Ensure previous resources are released

    m_internal->lastD3D11Device = d3d11DevicePtr;
    m_internal->lastUseHardware = useHardware;

    AVCodecID codecId = m_internal->isHEVC ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
    const AVCodec* codec = avcodec_find_decoder(codecId);
    if (!codec) {
        LOG_ERROR("Decoder", "Failed to find decoder for " + std::string(m_internal->isHEVC ? "HEVC" : "H.264"));
        return false;
    }

    m_internal->codecCtx = avcodec_alloc_context3(codec);
    if (!m_internal->codecCtx) {
        LOG_ERROR("Decoder", "Failed to allocate codec context.");
        return false;
    }

#ifdef _WIN32
    if (useHardware && d3d11DevicePtr) {
        m_internal->hwDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (m_internal->hwDeviceCtx) {
            AVHWDeviceContext* deviceCtx = (AVHWDeviceContext*)m_internal->hwDeviceCtx->data;
            if (deviceCtx) {
                AVD3D11VADeviceContext* d3d11Ctx = (AVD3D11VADeviceContext*)deviceCtx->hwctx;
                if (d3d11Ctx) {
                    d3d11Ctx->device = (ID3D11Device*)d3d11DevicePtr;
                    d3d11Ctx->device->AddRef();

                    if (av_hwdevice_ctx_init(m_internal->hwDeviceCtx) < 0) {
                        LOG_ERROR("Decoder", "Failed to initialize HW device context.");
                        av_buffer_unref(&m_internal->hwDeviceCtx);
                    } else {
                        m_internal->codecCtx->hw_device_ctx = av_buffer_ref(m_internal->hwDeviceCtx);
                        m_internal->codecCtx->get_format = get_format;
                    }
                } else {
                    LOG_ERROR("Decoder", "D3D11VADeviceContext is null.");
                    av_buffer_unref(&m_internal->hwDeviceCtx);
                }
            } else {
                LOG_ERROR("Decoder", "AVHWDeviceContext is null.");
                av_buffer_unref(&m_internal->hwDeviceCtx);
            }
        }
    }
#endif

    if (avcodec_open2(m_internal->codecCtx, codec, NULL) < 0) {
        LOG_ERROR("Decoder", "Failed to open codec.");
        return false;
    }

    m_internal->pkt = av_packet_alloc();
    m_internal->frame = av_frame_alloc();
    m_internal->tempFrame = av_frame_alloc();

    if (!m_internal->pkt || !m_internal->frame || !m_internal->tempFrame) {
        LOG_ERROR("Decoder", "Failed to allocate packet or frames.");
        return false;
    }

#ifdef _WIN32
    if (d3d11DevicePtr) {
        m_internal->device = (ID3D11Device*)d3d11DevicePtr;
        m_internal->device->AddRef();
        m_internal->device->GetImmediateContext(&m_internal->context);
        if (!m_internal->context) {
            LOG_ERROR("Decoder", "Failed to get immediate context.");
        }
    }
#endif

    LOG_INFO("Decoder", "FFmpeg Decoder initialized.");
    return true;
}

bool DecoderHW::DecodeFrame(const uint8_t* data, size_t size, void** outTexture, int* outIndex, bool isHEVC) {
    if (!m_internal || !m_internal->codecCtx) {
        LOG_ERROR("Decoder", "Decoder not initialized.");
        return false;
    }

    if (isHEVC != m_internal->isHEVC) {
        LOG_INFO("Decoder", "Codec change detected: " + std::string(m_internal->isHEVC ? "HEVC" : "H.264") + " -> " + std::string(isHEVC ? "HEVC" : "H.264"));
        m_internal->isHEVC = isHEVC;
        Shutdown();
        if (!Initialize(m_internal->lastD3D11Device, m_internal->lastUseHardware)) {
            LOG_ERROR("Decoder", "Re-initialization failed after codec change.");
            return false;
        }
    }

    if (!m_internal->codecCtx || !m_internal->pkt || !m_internal->frame) {
        LOG_ERROR("StreamTrace", "DECODER_INPUT_INVALID internal state null. size=" + std::to_string(size));
        return false;
    }
    LOG_INFO("StreamTrace", "DECODER_INPUT bytes=" + std::to_string(size) + " codec=" + std::string(isHEVC ? "HEVC" : "H.264"));

    if (outIndex) *outIndex = 0;

    // RESTORED: Log NAL units for debugging (only if suspecting issues or for keyframes)
    std::string nalTypes = "";
    if (size >= 4) {
        for (size_t i = 0; i + 3 < size; ) {
            size_t startCodeLen = 0;
            if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 0 && data[i+3] == 1) startCodeLen = 4;
            else if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 1) startCodeLen = 3;

            if (startCodeLen > 0 && i + startCodeLen < size) {
                int type = 0;
                if (m_internal->isHEVC) {
                    type = (data[i + startCodeLen] >> 1) & 0x3F;
                } else {
                    type = data[i + startCodeLen] & 0x1F;
                }
                if (!nalTypes.empty()) nalTypes += ",";
                nalTypes += std::to_string(type);
                i += startCodeLen + 1;
            } else {
                i++;
            }
        }
    }

    if (!m_internal->pkt) {
        LOG_ERROR("Decoder", "AVPacket is null during DecodeFrame.");
        return false;
    }

    LOG_INFO("Decoder", "Decoding frame: packet ptr=" + std::to_string(reinterpret_cast<uintptr_t>(m_internal->pkt)) + " size=" + std::to_string(size));

    av_packet_unref(m_internal->pkt);
    m_internal->pkt->data = (uint8_t*)data;
    m_internal->pkt->size = (int)size;

    int ret = avcodec_send_packet(m_internal->codecCtx, m_internal->pkt);
    if (ret == AVERROR(EAGAIN)) {
        int drainedCount = 0;
        while (avcodec_receive_frame(m_internal->codecCtx, m_internal->frame) >= 0) {
            drainedCount++;
        }
        LOG_WARN("Decoder", "avcodec_send_packet returned EAGAIN, drained " + std::to_string(drainedCount) + " frames. Retrying...");
        ret = avcodec_send_packet(m_internal->codecCtx, m_internal->pkt);
    }

    bool isKeyframe = false;
    if (m_internal->isHEVC) {
        if (nalTypes.find("19") != std::string::npos || nalTypes.find("20") != std::string::npos) isKeyframe = true;
    } else {
        if (nalTypes.find("5") != std::string::npos) isKeyframe = true;
    }

    if (isKeyframe || ret < 0) {
        LOG_INFO("StreamTrace", "AV_SEND_PACKET ret=" + std::to_string(ret) +
                 " bytes=" + std::to_string(size) + " NALs=[" + nalTypes + "]");
    }

    m_internal->pkt->data = nullptr;
    m_internal->pkt->size = 0;

    if (ret < 0) {
        LOG_ERROR("Decoder", "avcodec_send_packet failed (code: " + std::to_string(ret) + ")");
        return false;
    }

    if (!m_internal->tempFrame || !m_internal->frame) {
        LOG_ERROR("Decoder", "Internal AVFrames are null during DecodeFrame.");
        return false;
    }

    bool frameReady = false;
    while (true) {
        int recvRet = avcodec_receive_frame(m_internal->codecCtx, m_internal->tempFrame);
        if (recvRet == 0) {
            frameReady = true;
            av_frame_unref(m_internal->frame);
            av_frame_move_ref(m_internal->frame, m_internal->tempFrame);
            continue;
        } else if (recvRet == AVERROR(EAGAIN) || recvRet == AVERROR_EOF) {
            break;
        } else {
            LOG_ERROR("Decoder", "avcodec_receive_frame failed (code: " + std::to_string(recvRet) + ")");
            break;
        }
    }

    if (!frameReady) return false;

    LOG_INFO("Decoder", "Received decoded frame pointer=" + std::to_string(reinterpret_cast<uintptr_t>(m_internal->frame)) + " format=" + std::to_string(m_internal->frame->format));

    if (m_internal->frame->width <= 0 || m_internal->frame->height <= 0) {
        LOG_ERROR("Decoder", "Invalid frame dimensions: " + std::to_string(m_internal->frame->width) + "x" + std::to_string(m_internal->frame->height));
        return false;
    }

    LOG_INFO("StreamTrace", "AV_RECEIVE_FRAME_OK (latest) width=" + std::to_string(m_internal->frame->width) +
             " height=" + std::to_string(m_internal->frame->height) +
             " format=" + std::to_string(m_internal->frame->format));

#ifdef _WIN32
    if (m_internal->frame->format == AV_PIX_FMT_D3D11) {
        if (m_internal->frame->data[0]) {
            *outTexture = m_internal->frame->data[0];
            if (outIndex) *outIndex = (int)(intptr_t)m_internal->frame->data[1];
            LOG_INFO("StreamTrace", "DECODER_TEXTURE_OUT hw=1 texture=" + std::to_string(reinterpret_cast<uintptr_t>(*outTexture)));
            return true;
        } else {
            LOG_ERROR("StreamTrace", "DECODER_TEXTURE_OUT_FAIL hw_frame_data_null");
            return false;
        }
    } else {
        if (m_internal->device && m_internal->context) {
#endif
            if (!m_internal->swsCtx || !m_internal->rgbaFrame || m_internal->rgbaFrame->width != m_internal->frame->width || m_internal->rgbaFrame->height != m_internal->frame->height) {
                if (m_internal->swsCtx) sws_freeContext(m_internal->swsCtx);
                if (m_internal->rgbaFrame) av_frame_free(&m_internal->rgbaFrame);
#ifdef _WIN32
                if (m_internal->swTexture) { m_internal->swTexture->Release(); m_internal->swTexture = nullptr; }
#endif
                m_internal->swsCtx = sws_alloc_context();
                if (m_internal->swsCtx) {
                    av_opt_set_int(m_internal->swsCtx, "srcw", m_internal->frame->width, 0);
                    av_opt_set_int(m_internal->swsCtx, "srch", m_internal->frame->height, 0);
                    av_opt_set_int(m_internal->swsCtx, "src_format", m_internal->frame->format, 0);
                    av_opt_set_int(m_internal->swsCtx, "dstw", m_internal->frame->width, 0);
                    av_opt_set_int(m_internal->swsCtx, "dsth", m_internal->frame->height, 0);
                    av_opt_set_int(m_internal->swsCtx, "dst_format", AV_PIX_FMT_RGBA, 0);
                    av_opt_set_int(m_internal->swsCtx, "sws_flags", SWS_FAST_BILINEAR, 0);
                    sws_init_context(m_internal->swsCtx, nullptr, nullptr);
                }
                if (!m_internal->swsCtx) {
                    LOG_ERROR("Decoder", "Failed to create sws context.");
                    return false;
                }

                m_internal->rgbaFrame = av_frame_alloc();
                if (!m_internal->rgbaFrame) return false;
                m_internal->rgbaFrame->format = AV_PIX_FMT_RGBA;
                m_internal->rgbaFrame->width = m_internal->frame->width;
                m_internal->rgbaFrame->height = m_internal->frame->height;
                if (av_frame_get_buffer(m_internal->rgbaFrame, 0) < 0) {
                    LOG_ERROR("Decoder", "Failed to allocate software fallback buffer.");
                    return false;
                }

#ifdef _WIN32
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = m_internal->frame->width;
                desc.Height = m_internal->frame->height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                HRESULT hr = m_internal->device->CreateTexture2D(&desc, nullptr, &m_internal->swTexture);
                if (FAILED(hr)) {
                    LOG_ERROR("Decoder", "Failed to create fallback texture.");
                    return false;
                }
#endif
            }

            if (m_internal->swsCtx && m_internal->rgbaFrame && m_internal->frame) {
                sws_scale_frame(m_internal->swsCtx, m_internal->rgbaFrame, m_internal->frame);
            } else {
                LOG_ERROR("Decoder", "sws_scale_frame parameters are invalid or null.");
                return false;
            }

#ifdef _WIN32
            if (m_internal->context && m_internal->swTexture && m_internal->rgbaFrame && m_internal->rgbaFrame->data[0]) {
                m_internal->context->UpdateSubresource(m_internal->swTexture, 0, nullptr, m_internal->rgbaFrame->data[0], m_internal->rgbaFrame->linesize[0], 0);
                *outTexture = m_internal->swTexture;
                LOG_INFO("StreamTrace", "DECODER_TEXTURE_OUT hw=0 texture=" + std::to_string(reinterpret_cast<uintptr_t>(*outTexture)));
            } else {
                LOG_ERROR("Decoder", "D3D11 Context, Software Texture, or RGBA Frame data is null during software scaling.");
                return false;
            }
        } else {
            LOG_ERROR("StreamTrace", "DECODER_TEXTURE_OUT_FAIL no_d3d11_device_or_context_for_software_frame");
            return false;
        }
    }
#endif
    return true;
}

bool DecoderHW::IsHardware() const {
    return m_internal && m_internal->codecCtx && m_internal->codecCtx->hw_device_ctx != nullptr;
}

void DecoderHW::Shutdown() {
    if (m_internal->swsCtx) sws_freeContext(m_internal->swsCtx);
    if (m_internal->rgbaFrame) av_frame_free(&m_internal->rgbaFrame);
#ifdef _WIN32
    if (m_internal->swTexture) m_internal->swTexture->Release();
    if (m_internal->context) m_internal->context->Release();
    if (m_internal->device) m_internal->device->Release();
#endif
    m_internal->swsCtx = nullptr;
    m_internal->rgbaFrame = nullptr;
#ifdef _WIN32
    m_internal->swTexture = nullptr;
    m_internal->context = nullptr;
    m_internal->device = nullptr;
#endif

    if (m_internal->codecCtx) {
        avcodec_free_context(&m_internal->codecCtx);
    }
    if (m_internal->hwDeviceCtx) av_buffer_unref(&m_internal->hwDeviceCtx);
    if (m_internal->frame) av_frame_free(&m_internal->frame);
    if (m_internal->tempFrame) av_frame_free(&m_internal->tempFrame);
    if (m_internal->pkt) av_packet_free(&m_internal->pkt);
}
#else
struct DecoderHW::InternalData {};
DecoderHW::DecoderHW() : m_internal(nullptr) {}
DecoderHW::~DecoderHW() {}
bool DecoderHW::Initialize(void* d3d11DevicePtr, bool useHardware) { return false; }
bool DecoderHW::DecodeFrame(const uint8_t* d, size_t s, void** o, int* i, bool h) { return false; }
bool DecoderHW::IsHardware() const { return false; }
void DecoderHW::Shutdown() {}
#endif

} // namespace Client
