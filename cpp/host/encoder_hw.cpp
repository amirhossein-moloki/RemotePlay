#include "encoder_hw.hpp"
#include <iostream>
#include <algorithm>
#include "../common/logger.hpp"

#ifdef PARSEC_LITE_ENABLE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libswscale/swscale.h>
}
#endif
#include <cstdint>

namespace Host {

#ifdef PARSEC_LITE_ENABLE_FFMPEG
struct FFmpegHardwareEncoder::InternalData {
    AVCodecContext* codecCtx = nullptr;
    AVBufferRef* hwDeviceCtx = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;
    int64_t frameCounter = 0;

    // Software fallback resources
    ID3D11Device* d3d11Device = nullptr;
    ID3D11DeviceContext* d3d11Context = nullptr;
    ID3D11Texture2D* stagingTex = nullptr;
    SwsContext* swsCtx = nullptr;
    AVFrame* swFrame = nullptr;
};

FFmpegHardwareEncoder::FFmpegHardwareEncoder() : m_internal(new InternalData()) {}

FFmpegHardwareEncoder::~FFmpegHardwareEncoder() {
    Shutdown();
    delete m_internal;
}

bool FFmpegHardwareEncoder::Initialize(int width, int height, int fps, int bitrateKbps, void* d3d11Device, int preset, const std::string& codecName) {
    Shutdown(); // Reset state

    m_width = width;
    m_height = height;
    m_fps = fps;
    m_bitrate = bitrateKbps;

    if (d3d11Device) {
        m_internal->d3d11Device = (ID3D11Device*)d3d11Device;
        m_internal->d3d11Device->AddRef();
        m_internal->d3d11Device->GetImmediateContext(&m_internal->d3d11Context);
    }

    auto tryInitialize = [&](bool softwareFallback) -> bool {
        const AVCodec* codec = nullptr;
        bool isSoftware = false;

        if (!codecName.empty()) {
            codec = avcodec_find_encoder_by_name(codecName.c_str());
            if (codec) {
                std::string name = codec->name;
                if (name == "libx264" || name == "h264" || name == "libx265" || name == "hevc") isSoftware = true;
                // If it's x264/x265 but not one of the known hardware ones, treat as software
                if ((name.find("264") != std::string::npos || name.find("265") != std::string::npos || name.find("hevc") != std::string::npos) &&
                    name.find("nvenc") == std::string::npos &&
                    name.find("amf") == std::string::npos &&
                    name.find("qsv") == std::string::npos &&
                    name.find("vaapi") == std::string::npos) {
                    isSoftware = true;
                }
            }
        }

        if (!codec && !softwareFallback) {
            codec = avcodec_find_encoder_by_name("hevc_nvenc");
            if (!codec) codec = avcodec_find_encoder_by_name("h264_nvenc");
            if (!codec) codec = avcodec_find_encoder_by_name("hevc_amf");
            if (!codec) codec = avcodec_find_encoder_by_name("h264_amf");
            if (!codec) codec = avcodec_find_encoder_by_name("hevc_qsv");
            if (!codec) codec = avcodec_find_encoder_by_name("h264_qsv");
        }

        if (!codec) {
            if (!softwareFallback) LOG_WARN("Encoder", "No requested or hardware H.264 encoder found. Trying software...");
            codec = avcodec_find_encoder_by_name("libx264");
            if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_H264);
            isSoftware = true;
        }

        if (!codec) return false;

        m_internal->codecCtx = avcodec_alloc_context3(codec);
        if (!m_internal->codecCtx) return false;

        m_internal->codecCtx->width = width;
        m_internal->codecCtx->height = height;
        m_internal->codecCtx->time_base = { 1, fps };
        m_internal->codecCtx->framerate = { fps, 1 };
        m_internal->codecCtx->pix_fmt = isSoftware ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_NV12;
        m_internal->codecCtx->bit_rate = (int64_t)bitrateKbps * 1000;
        m_internal->codecCtx->gop_size = 60;
        m_internal->codecCtx->max_b_frames = 0;

        if (isSoftware) {
            const char* sw_presets[] = { "ultrafast", "superfast", "veryfast" };
            av_opt_set(m_internal->codecCtx->priv_data, "preset", sw_presets[std::max(0, std::min(2, preset))], 0);
            av_opt_set(m_internal->codecCtx->priv_data, "tune", "zerolatency", 0);
            if (codec->id == AV_CODEC_ID_H264) {
                av_opt_set(m_internal->codecCtx->priv_data, "x264-params", "repeat-headers=1:annexb=1", 0);
            } else if (codec->id == AV_CODEC_ID_HEVC) {
                av_opt_set(m_internal->codecCtx->priv_data, "x265-params", "repeat-headers=1:annexb=1", 0);
            }
        } else {
            const char* hw_presets[] = { "p1", "p4", "p7" }; // NVENC: p1 is fastest, p7 is slowest/best
            if (std::string(codec->name).find("nvenc") != std::string::npos) {
                av_opt_set(m_internal->codecCtx->priv_data, "preset", hw_presets[std::max(0, std::min(2, preset))], 0);
                av_opt_set(m_internal->codecCtx->priv_data, "tune", "ull", 0);
                av_opt_set(m_internal->codecCtx->priv_data, "repeat_config", "1", 0);
                av_opt_set(m_internal->codecCtx->priv_data, "forced-idr", "1", 0);
            } else {
                const char* qsv_presets[] = { "veryfast", "medium", "veryslow" };
                const char* amf_presets[] = { "speed", "balanced", "quality" };
                const char* p = "balanced";

                if (std::string(codec->name).find("qsv") != std::string::npos) {
                    p = qsv_presets[std::max(0, std::min(2, preset))];
                    av_opt_set(m_internal->codecCtx->priv_data, "preset", p, 0);
                    av_opt_set(m_internal->codecCtx->priv_data, "tune", "zerolatency", 0);
                    av_opt_set(m_internal->codecCtx->priv_data, "repeat_headers", "1", 0);
                    av_opt_set(m_internal->codecCtx->priv_data, "extra_hw_frames", "2", 0);
                    av_opt_set(m_internal->codecCtx->priv_data, "async_depth", "1", 0);
                } else {
                    p = amf_presets[std::max(0, std::min(2, preset))];
                    av_opt_set(m_internal->codecCtx->priv_data, "preset", p, 0);
                    av_opt_set(m_internal->codecCtx->priv_data, "tune", "zerolatency", 0);
                    av_opt_set(m_internal->codecCtx->priv_data, "header_insertion_mode", "1", 0);
                    av_opt_set(m_internal->codecCtx->priv_data, "query_timeout", "1000", 0);
                }
            }
        }
        av_opt_set(m_internal->codecCtx->priv_data, "rc", "cbr", 0);

        if (d3d11Device && !isSoftware) {
            bool supportsD3D11 = false;
            const enum AVPixelFormat* pix_fmts = nullptr;
            if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, (const void**)&pix_fmts, nullptr) >= 0 && pix_fmts) {
                for (const enum AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
                    if (*p == AV_PIX_FMT_D3D11) {
                        supportsD3D11 = true;
                        break;
                    }
                }
            }

            if (supportsD3D11) {
                m_internal->codecCtx->pix_fmt = AV_PIX_FMT_D3D11;
                AVBufferRef* device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
                if (device_ref) {
                    AVHWDeviceContext* device_ctx = (AVHWDeviceContext*)device_ref->data;
                    AVD3D11VADeviceContext* d3d11_ctx = (AVD3D11VADeviceContext*)device_ctx->hwctx;
                    d3d11_ctx->device = (ID3D11Device*)d3d11Device;
                    d3d11_ctx->device->AddRef();

                    if (av_hwdevice_ctx_init(device_ref) >= 0) {
                        m_internal->hwDeviceCtx = device_ref;
                        m_internal->codecCtx->hw_device_ctx = av_buffer_ref(device_ref);

                        AVBufferRef* frames_ref = av_hwframe_ctx_alloc(device_ref);
                        AVHWFramesContext* frames_ctx = (AVHWFramesContext*)frames_ref->data;
                        frames_ctx->format = AV_PIX_FMT_D3D11;
                        frames_ctx->sw_format = AV_PIX_FMT_NV12;
                        frames_ctx->width = width;
                        frames_ctx->height = height;
                        frames_ctx->initial_pool_size = 0;

                        if (av_hwframe_ctx_init(frames_ref) >= 0) {
                            m_internal->codecCtx->hw_frames_ctx = av_buffer_ref(frames_ref);
                            m_internal->codecCtx->pix_fmt = AV_PIX_FMT_D3D11;
                        }
                        av_buffer_unref(&frames_ref);
                    } else {
                        av_buffer_unref(&device_ref);
                    }
                }
            } else {
                LOG_WARN("Encoder", "Codec " + std::string(codec->name) + " does not support D3D11 pixel format directly. Using software-to-hardware upload path.");
            }
        }

        if (avcodec_open2(m_internal->codecCtx, codec, NULL) < 0) {
            Shutdown(); // Clean up before retrying or failing
            return false;
        }

    // Always allocate swFrame if we might need scaling or software fallback
    m_internal->swFrame = av_frame_alloc();
    m_internal->swFrame->format = isSoftware ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_NV12;
    m_internal->swFrame->width = width;
    m_internal->swFrame->height = height;
    av_frame_get_buffer(m_internal->swFrame, 0);

        return true;
    };

    if (!tryInitialize(false)) {
        LOG_ERROR("Encoder", "Failed to initialize requested backend.");
        return false;
    }

    m_internal->pkt = av_packet_alloc();
    m_internal->frame = av_frame_alloc();

    m_initialized = true;
    return true;
}

bool FFmpegHardwareEncoder::EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets, PacketPool& pool) {
    if (!m_internal->codecCtx || !texturePtr) {
        LOG_ERROR("StreamTrace", "ENCODER_HW_INPUT_INVALID codecCtx=" + std::to_string(reinterpret_cast<uintptr_t>(m_internal->codecCtx)) +
                  " texture=" + std::to_string(reinterpret_cast<uintptr_t>(texturePtr)));
        return false;
    }

    LOG_INFO("StreamTrace", "ENCODER_HW_INPUT texture=" + std::to_string(reinterpret_cast<uintptr_t>(texturePtr)) +
             " target=" + std::to_string(m_width) + "x" + std::to_string(m_height));

    AVFrame* encodeFrame = m_internal->frame;
    av_frame_unref(encodeFrame);

    if (m_internal->codecCtx->hw_frames_ctx) {
        // Zero-copy path: Wrap the D3D11 texture directly
        // Note: This only works if captured resolution matches encoder resolution.
        // If scaling was applied in SessionManager, m_width/m_height will be scaled.

        D3D11_TEXTURE2D_DESC desc;
        ((ID3D11Texture2D*)texturePtr)->GetDesc(&desc);

        LOG_INFO("StreamTrace", "ENCODER_HW_TEXTURE_DESC width=" + std::to_string(desc.Width) +
                 " height=" + std::to_string(desc.Height) +
                 " format=" + std::to_string(desc.Format));

        if (desc.Width == (UINT)m_width && desc.Height == (UINT)m_height) {
            encodeFrame->data[0] = (uint8_t*)texturePtr;
            encodeFrame->format = AV_PIX_FMT_D3D11;
            encodeFrame->hw_frames_ctx = av_buffer_ref(m_internal->codecCtx->hw_frames_ctx);
        } else {
            // We need to scale. For now, hardware path without zero-copy
            // or implementing D3D11 scaling.
            // Simplified: if resolutions don't match and we're in "hardware" mode,
            // fallback to software scaling for now or log error.
            LOG_WARN("Encoder", "Resolution mismatch in hardware path. Falling back to software scaling.");
            goto software_path;
        }
    } else {
    software_path:
        if (m_internal->swFrame) {
        // Software fallback path: GPU -> Staging Texture -> CPU -> sws_scale -> software encoder
        D3D11_TEXTURE2D_DESC desc;
        ((ID3D11Texture2D*)texturePtr)->GetDesc(&desc);

        if (!m_internal->stagingTex) {
            D3D11_TEXTURE2D_DESC stagingDesc = desc;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.BindFlags = 0;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            stagingDesc.MiscFlags = 0;
            m_internal->d3d11Device->CreateTexture2D(&stagingDesc, nullptr, &m_internal->stagingTex);
        }

        if (m_internal->stagingTex && m_internal->d3d11Context) {
            // Check for device loss BEFORE copying
            if (m_internal->d3d11Device) {
                HRESULT hr = m_internal->d3d11Device->GetDeviceRemovedReason();
                if (FAILED(hr)) {
                    char hex[16]; snprintf(hex, sizeof(hex), "0x%08X", (uint32_t)hr);
                    LOG_ERROR("Encoder", "D3D11 Device Lost before CopyResource! Reason: " + std::string(hex));
                    return false;
                }
            }

            m_internal->d3d11Context->CopyResource(m_internal->stagingTex, (ID3D11Texture2D*)texturePtr);

            if (m_internal->d3d11Device) {
                HRESULT hr = m_internal->d3d11Device->GetDeviceRemovedReason();
                if (FAILED(hr)) {
                    char hex[16]; snprintf(hex, sizeof(hex), "0x%08X", (uint32_t)hr);
                    LOG_ERROR("Encoder", "D3D11 Device Lost during CopyResource! Reason: " + std::string(hex));
                    return false;
                }
            }

            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(m_internal->d3d11Context->Map(m_internal->stagingTex, 0, D3D11_MAP_READ, 0, &mapped))) {
                if (!m_internal->swsCtx && m_internal->swFrame) {
                    m_internal->swsCtx = sws_alloc_context();
                    if (m_internal->swsCtx) {
                        av_opt_set_int(m_internal->swsCtx, "srcw", desc.Width, 0);
                        av_opt_set_int(m_internal->swsCtx, "srch", desc.Height, 0);
                        av_opt_set_int(m_internal->swsCtx, "src_format", AV_PIX_FMT_BGRA, 0);
                        av_opt_set_int(m_internal->swsCtx, "dstw", m_width, 0);
                        av_opt_set_int(m_internal->swsCtx, "dsth", m_height, 0);
                        av_opt_set_int(m_internal->swsCtx, "dst_format", m_internal->swFrame->format, 0);
                        av_opt_set_int(m_internal->swsCtx, "sws_flags", SWS_FAST_BILINEAR, 0);
                        if (sws_init_context(m_internal->swsCtx, nullptr, nullptr) < 0) {
                            sws_freeContext(m_internal->swsCtx);
                            m_internal->swsCtx = nullptr;
                        }
                    }
                }

                if (m_internal->swsCtx) {
                    AVFrame* wrapFrame = av_frame_alloc();
                    wrapFrame->format = AV_PIX_FMT_BGRA;
                    wrapFrame->width = desc.Width;
                    wrapFrame->height = desc.Height;
                    wrapFrame->data[0] = (uint8_t*)mapped.pData;
                    wrapFrame->linesize[0] = (int)mapped.RowPitch;

                    sws_scale_frame(m_internal->swsCtx, m_internal->swFrame, wrapFrame);
                    av_frame_free(&wrapFrame);
                }

                m_internal->d3d11Context->Unmap(m_internal->stagingTex, 0);

                if (m_internal->codecCtx->hw_frames_ctx) {
                    // Upload swFrame to hwFrame
                    AVFrame* hwFrame = av_frame_alloc();
                    av_hwframe_get_buffer(m_internal->codecCtx->hw_frames_ctx, hwFrame, 0);
                    av_hwframe_transfer_data(hwFrame, m_internal->swFrame, 0);
                    encodeFrame = m_internal->frame;
                    av_frame_unref(encodeFrame);
                    av_frame_ref(encodeFrame, hwFrame);
                    av_frame_free(&hwFrame);
                } else {
                    encodeFrame = m_internal->swFrame;
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        LOG_ERROR("Encoder", "Encoder not initialized correctly for hardware or software path.");
        return false;
    }
    }

    encodeFrame->width = m_internal->codecCtx->width;
    encodeFrame->height = m_internal->codecCtx->height;
    encodeFrame->format = m_internal->codecCtx->pix_fmt;
    encodeFrame->color_range = AVCOL_RANGE_MPEG;
    encodeFrame->colorspace = AVCOL_SPC_BT709;
    encodeFrame->color_primaries = AVCOL_PRI_BT709;
    encodeFrame->color_trc = AVCOL_TRC_BT709;
    encodeFrame->pts = m_internal->frameCounter++;

    if (m_forceKeyframe) {
        encodeFrame->pict_type = AV_PICTURE_TYPE_I;
        encodeFrame->flags |= AV_FRAME_FLAG_KEY;
        m_forceKeyframe = false;
    } else {
        encodeFrame->pict_type = AV_PICTURE_TYPE_NONE;
        encodeFrame->flags &= ~AV_FRAME_FLAG_KEY;
    }

    int ret = avcodec_send_frame(m_internal->codecCtx, encodeFrame);
    LOG_INFO("StreamTrace", "AV_SEND_FRAME ret=" + std::to_string(ret) +
             " pts=" + std::to_string(encodeFrame->pts) +
             " frameCounter=" + std::to_string(m_internal->frameCounter));
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        LOG_ERROR("StreamTrace", "AV_SEND_FRAME_FAIL ret=" + std::to_string(ret) + " (" + std::string(errBuf) + ") codec=" + m_internal->codecCtx->codec->name);
    }
    if (encodeFrame == m_internal->frame) {
        // We only unref if it was the wrapper frame. The software frame is reused.
        av_frame_unref(encodeFrame);
    }
    if (ret < 0) return false;

    std::vector<uint8_t> frameData;
    bool isKeyframe = false;

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_internal->codecCtx, m_internal->pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) {
            LOG_ERROR("StreamTrace", "AV_RECEIVE_PACKET_FAIL ret=" + std::to_string(ret));
            return false;
        }

        if (m_internal->pkt->flags & AV_PKT_FLAG_KEY) isKeyframe = true;

        size_t oldSize = frameData.size();
        frameData.resize(oldSize + m_internal->pkt->size);
        memcpy(frameData.data() + oldSize, m_internal->pkt->data, m_internal->pkt->size);

        av_packet_unref(m_internal->pkt);
    }

    if (!frameData.empty()) {
        auto pktBuffer = pool.acquire();
        if (pktBuffer) {
            if (frameData.size() <= pktBuffer->data.size()) {
                memcpy(pktBuffer->data.data(), frameData.data(), frameData.size());
                pktBuffer->size = frameData.size();

                EncodedPacket ep;
                ep.packet = std::move(pktBuffer);
                ep.isKeyframe = isKeyframe;
                ep.captureTimestamp = 0;
                ep.encodeStartTimestamp = 0;
                ep.encodeEndTimestamp = 0;

                if (outPackets.capacity() < outPackets.size() + 1) {
                    outPackets.reserve(std::max((size_t)8, outPackets.capacity() * 2));
                }
                outPackets.push_back(std::move(ep));

                // Log NAL units for debugging (only for keyframes or intermittently to reduce noise)
                if (isKeyframe && frameData.size() >= 4) {
                    std::string nalTypes = "";
                    for (size_t i = 0; i + 3 < frameData.size(); ) {
                        size_t startCodeLen = 0;
                        if (frameData[i] == 0 && frameData[i+1] == 0 && frameData[i+2] == 0 && frameData[i+3] == 1) startCodeLen = 4;
                        else if (frameData[i] == 0 && frameData[i+1] == 0 && frameData[i+2] == 1) startCodeLen = 3;

                        if (startCodeLen > 0 && i + startCodeLen < frameData.size()) {
                            int type = 0;
                            if (m_internal->codecCtx->codec_id == AV_CODEC_ID_HEVC) {
                                type = (frameData[i + startCodeLen] >> 1) & 0x3F;
                            } else {
                                type = frameData[i + startCodeLen] & 0x1F;
                            }
                            if (!nalTypes.empty()) nalTypes += ",";
                            nalTypes += std::to_string(type);
                            i += startCodeLen + 1;
                        } else {
                            i++;
                        }
                    }
                    LOG_INFO("StreamTrace", "AV_RECEIVE_FRAME_BUNDLED size=" + std::to_string(frameData.size()) +
                             " keyframe=" + std::to_string(isKeyframe) + " NALs=[" + nalTypes + "]");
                } else {
                    LOG_INFO("StreamTrace", "AV_RECEIVE_FRAME_BUNDLED size=" + std::to_string(frameData.size()) +
                             " keyframe=" + std::to_string(isKeyframe));
                }
            } else {
                LOG_ERROR("StreamTrace", "ENCODED_FRAME_TOO_LARGE size=" + std::to_string(frameData.size()) +
                          " poolCapacity=" + std::to_string(pktBuffer->data.size()));
                pool.release(std::move(pktBuffer));
            }
        }
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

void FFmpegHardwareEncoder::ForceKeyframe() {
    m_forceKeyframe = true;
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

    if (m_internal->swsCtx) { sws_freeContext(m_internal->swsCtx); m_internal->swsCtx = nullptr; }
    if (m_internal->swFrame) { av_frame_free(&m_internal->swFrame); m_internal->swFrame = nullptr; }
    if (m_internal->stagingTex) { m_internal->stagingTex->Release(); m_internal->stagingTex = nullptr; }
    if (m_internal->d3d11Context) { m_internal->d3d11Context->Release(); m_internal->d3d11Context = nullptr; }
    if (m_internal->d3d11Device) { m_internal->d3d11Device->Release(); m_internal->d3d11Device = nullptr; }
}

#else
struct FFmpegHardwareEncoder::InternalData {};
FFmpegHardwareEncoder::FFmpegHardwareEncoder() : m_internal(nullptr) {}
FFmpegHardwareEncoder::~FFmpegHardwareEncoder() {}
bool FFmpegHardwareEncoder::Initialize(int w, int h, int f, int b, void* d, int preset, const std::string& codecName) {
    LOG_ERROR("Encoder", "FFmpeg support not compiled in. Hardware encoding is disabled.");
    return false;
}
bool FFmpegHardwareEncoder::EncodeFrame(void* t, std::vector<EncodedPacket>& o, PacketPool& p) { return false; }
void FFmpegHardwareEncoder::SetBitrate(int b) {}
void FFmpegHardwareEncoder::ForceKeyframe() {}
void FFmpegHardwareEncoder::Shutdown() {}
#endif

} // namespace Host
