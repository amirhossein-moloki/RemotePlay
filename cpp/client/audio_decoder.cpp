#include "audio_decoder.hpp"
#include "../common/logger.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
}

namespace Client {

struct AudioDecoder::InternalData {
    AVCodecContext* codecCtx = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;

    ~InternalData() {
        if (codecCtx) avcodec_free_context(&codecCtx);
        if (frame) av_frame_free(&frame);
        if (packet) av_packet_free(&packet);
    }
};

AudioDecoder::AudioDecoder() : m_internal(new InternalData()) {}

AudioDecoder::~AudioDecoder() {
    Shutdown();
    delete m_internal;
}

bool AudioDecoder::Initialize(int sampleRate, int channels) {
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    if (!codec) return false;

    m_internal->codecCtx = avcodec_alloc_context3(codec);
    m_internal->codecCtx->sample_rate = sampleRate;

    AVChannelLayout layout;
    if (channels == 1) layout = AV_CHANNEL_LAYOUT_MONO;
    else layout = AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy(&m_internal->codecCtx->ch_layout, &layout);

    if (avcodec_open2(m_internal->codecCtx, codec, NULL) < 0) return false;

    m_internal->frame = av_frame_alloc();
    m_internal->packet = av_packet_alloc();

    return true;
}

bool AudioDecoder::Decode(const uint8_t* opusData, size_t size, std::vector<float>& outPcmData) {
    if (!m_internal->codecCtx) return false;

    m_internal->packet->data = (uint8_t*)opusData;
    m_internal->packet->size = (int)size;

    if (avcodec_send_packet(m_internal->codecCtx, m_internal->packet) < 0) return false;

    while (avcodec_receive_frame(m_internal->codecCtx, m_internal->frame) == 0) {
        int channels = m_internal->frame->ch_layout.nb_channels;
        int nb_samples = m_internal->frame->nb_samples;

        // Planar Float to Interleaved Float conversion
        size_t offset = outPcmData.size();
        outPcmData.resize(offset + (size_t)nb_samples * channels);
        for (int i = 0; i < nb_samples; i++) {
            for (int ch = 0; ch < channels; ch++) {
                float* channelData = (float*)m_internal->frame->data[ch];
                outPcmData[offset + (size_t)i * channels + ch] = channelData[i];
            }
        }
    }

    return !outPcmData.empty();
}

void AudioDecoder::Shutdown() {
    // Handled by InternalData destructor
}

} // namespace Client
