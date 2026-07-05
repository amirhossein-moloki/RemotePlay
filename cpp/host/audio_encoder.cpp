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
    if (!codec) return false;

    m_internal->codecCtx = avcodec_alloc_context3(codec);
    m_internal->codecCtx->bit_rate = bitrate;
    m_internal->codecCtx->sample_rate = sampleRate;
    m_internal->codecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP; // Opus expects Float Planar

    AVChannelLayout layout;
    if (channels == 1) layout = AV_CHANNEL_LAYOUT_MONO;
    else layout = AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy(&m_internal->codecCtx->ch_layout, &layout);

    if (avcodec_open2(m_internal->codecCtx, codec, NULL) < 0) return false;

    m_internal->frame = av_frame_alloc();
    m_internal->frame->nb_samples = m_internal->codecCtx->frame_size;
    m_internal->frame->format = m_internal->codecCtx->sample_fmt;
    av_channel_layout_copy(&m_internal->frame->ch_layout, &m_internal->codecCtx->ch_layout);
    av_frame_get_buffer(m_internal->frame, 0);

    m_internal->packet = av_packet_alloc();

    return true;
}

bool AudioEncoder::Encode(const float* pcmData, size_t samples, std::vector<uint8_t>& outOpusData) {
    if (!m_internal->codecCtx) return false;

    int channels = m_internal->codecCtx->ch_layout.nb_channels;
    int nb_samples = (int)samples / channels;

    if (nb_samples != m_internal->codecCtx->frame_size) {
        // In a real system, we'd use a FIFO buffer here to accumulate enough samples for one Opus frame.
        // For now, assume samples matches frame_size.
    }

    // Interleaved Float to Planar Float conversion
    for (int i = 0; i < nb_samples; i++) {
        for (int ch = 0; ch < channels; ch++) {
            float* channelData = (float*)m_internal->frame->data[ch];
            channelData[i] = pcmData[i * channels + ch];
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
