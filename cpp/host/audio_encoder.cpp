#include "audio_encoder.hpp"
#include "../common/logger.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}

namespace Host {

struct AudioEncoder::InternalData {
    AVCodecContext* codecCtx = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    AVSampleFormat selectedFmt = AV_SAMPLE_FMT_NONE;

    ~InternalData() {
        if (codecCtx) avcodec_free_context(&codecCtx);
        if (frame) av_frame_free(&frame);
        if (packet) av_packet_free(&packet);
    }
};

AudioEncoder::AudioEncoder() : m_internal(new InternalData()) {}

AudioEncoder::~AudioEncoder() {
    Shutdown();
    delete m_internal;
}

bool AudioEncoder::Initialize(int sampleRate, int channels, int bitrate) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!codec) {
        LOG_ERROR("AudioEncoder", "Opus encoder not found.");
        return false;
    }

    m_internal->codecCtx = avcodec_alloc_context3(codec);
    if (!m_internal->codecCtx) {
        LOG_ERROR("AudioEncoder", "Failed to allocate codec context.");
        return false;
    }

    m_internal->codecCtx->bit_rate = bitrate;
    m_internal->codecCtx->sample_rate = sampleRate;

    // Dynamically select the best supported sample format for this Opus encoder
    AVSampleFormat bestFmt = AV_SAMPLE_FMT_FLT; // Default fallback
    if (codec->sample_fmts) {
        bool fltSupported = false;
        bool fltpSupported = false;
        bool s16Supported = false;
        for (const enum AVSampleFormat* p = codec->sample_fmts; *p != AV_SAMPLE_FMT_NONE; p++) {
            if (*p == AV_SAMPLE_FMT_FLT) fltSupported = true;
            if (*p == AV_SAMPLE_FMT_FLTP) fltpSupported = true;
            if (*p == AV_SAMPLE_FMT_S16) s16Supported = true;
        }
        if (fltSupported) bestFmt = AV_SAMPLE_FMT_FLT;
        else if (fltpSupported) bestFmt = AV_SAMPLE_FMT_FLTP;
        else if (s16Supported) bestFmt = AV_SAMPLE_FMT_S16;
        else bestFmt = codec->sample_fmts[0];
    }
    m_internal->selectedFmt = bestFmt;
    m_internal->codecCtx->sample_fmt = bestFmt;

    AVChannelLayout layout;
    if (channels == 1) layout = AV_CHANNEL_LAYOUT_MONO;
    else layout = AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy(&m_internal->codecCtx->ch_layout, &layout);

    if (avcodec_open2(m_internal->codecCtx, codec, NULL) < 0) {
        LOG_ERROR("AudioEncoder", "Failed to open Opus codec context.");
        return false;
    }

    m_internal->frame = av_frame_alloc();
    if (!m_internal->frame) {
        LOG_ERROR("AudioEncoder", "Failed to allocate frame.");
        return false;
    }

    m_internal->frame->nb_samples = m_internal->codecCtx->frame_size;
    m_internal->frame->format = m_internal->codecCtx->sample_fmt;
    av_channel_layout_copy(&m_internal->frame->ch_layout, &m_internal->codecCtx->ch_layout);

    if (av_frame_get_buffer(m_internal->frame, 0) < 0) {
        LOG_ERROR("AudioEncoder", "Failed to get frame buffer.");
        return false;
    }

    m_internal->packet = av_packet_alloc();
    if (!m_internal->packet) {
        LOG_ERROR("AudioEncoder", "Failed to allocate packet.");
        return false;
    }

    return true;
}

bool AudioEncoder::Encode(const float* pcmData, size_t samples, std::vector<uint8_t>& outOpusData) {
    if (!m_internal->codecCtx || !m_internal->frame) return false;

    int channels = m_internal->codecCtx->ch_layout.nb_channels;
    if (channels <= 0) return false; // Prevent divide by zero

    int nb_samples = (int)samples / channels;

    if (nb_samples != m_internal->codecCtx->frame_size) {
        // In a real system, we'd use a FIFO buffer here to accumulate enough samples for one Opus frame.
        // For now, assume samples matches frame_size.
    }

    // Convert Interleaved Float PCM input to selected frame sample format
    if (m_internal->selectedFmt == AV_SAMPLE_FMT_FLT) {
        // Interleaved Float to Interleaved Float (direct copy)
        float* dest = (float*)m_internal->frame->data[0];
        if (dest && pcmData) {
            memcpy(dest, pcmData, samples * sizeof(float));
        }
    } else if (m_internal->selectedFmt == AV_SAMPLE_FMT_FLTP) {
        // Interleaved Float to Planar Float conversion
        for (int i = 0; i < nb_samples; i++) {
            for (int ch = 0; ch < channels; ch++) {
                float* channelData = (float*)m_internal->frame->data[ch];
                if (channelData) {
                    channelData[i] = pcmData[i * channels + ch];
                }
            }
        }
    } else if (m_internal->selectedFmt == AV_SAMPLE_FMT_S16) {
        // Interleaved Float to Interleaved S16 conversion
        int16_t* dest = (int16_t*)m_internal->frame->data[0];
        if (dest && pcmData) {
            for (size_t i = 0; i < samples; i++) {
                float f = pcmData[i];
                if (f > 1.0f) f = 1.0f;
                else if (f < -1.0f) f = -1.0f;
                dest[i] = (int16_t)(f * 32767.0f);
            }
        }
    } else if (m_internal->selectedFmt == AV_SAMPLE_FMT_S16P) {
        // Interleaved Float to Planar S16 conversion
        for (int i = 0; i < nb_samples; i++) {
            for (int ch = 0; ch < channels; ch++) {
                int16_t* channelData = (int16_t*)m_internal->frame->data[ch];
                if (channelData) {
                    float f = pcmData[i * channels + ch];
                    if (f > 1.0f) f = 1.0f;
                    else if (f < -1.0f) f = -1.0f;
                    channelData[i] = (int16_t)(f * 32767.0f);
                }
            }
        }
    } else {
        // Unknown format, attempt generic float copy to first plane
        float* dest = (float*)m_internal->frame->data[0];
        if (dest && pcmData) {
            memcpy(dest, pcmData, std::min(samples * sizeof(float), (size_t)m_internal->frame->linesize[0]));
        }
    }

    if (avcodec_send_frame(m_internal->codecCtx, m_internal->frame) < 0) return false;

    while (avcodec_receive_packet(m_internal->codecCtx, m_internal->packet) == 0) {
        outOpusData.insert(outOpusData.end(), m_internal->packet->data, m_internal->packet->data + m_internal->packet->size);
        av_packet_unref(m_internal->packet);
    }

    return !outOpusData.empty();
}

void AudioEncoder::Shutdown() {
    // Handled by InternalData destructor
}

} // namespace Host
