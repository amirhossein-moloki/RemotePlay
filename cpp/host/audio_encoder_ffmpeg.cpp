#include "audio_encoder_ffmpeg.hpp"
#include "common/logger.hpp"

namespace Host {

FFmpegAudioEncoder::FFmpegAudioEncoder() {}

FFmpegAudioEncoder::~FFmpegAudioEncoder() {
    Shutdown();
}

bool FFmpegAudioEncoder::Initialize(const Audio::AudioFormat& format, int bitrateKbps) {
    m_sourceFormat = format;

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!codec) {
        LOG_ERROR("AudioEncoder", "Opus encoder not found in FFmpeg");
        return false;
    }

    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) return false;

    m_codecContext->bit_rate = bitrateKbps * 1000;
    m_codecContext->sample_fmt = AV_SAMPLE_FMT_FLTP; // Opus usually uses float planar
    m_codecContext->sample_rate = 48000;
    m_codecContext->channel_layout = AV_CH_LAYOUT_STEREO;
    m_codecContext->channels = 2;
    m_codecContext->time_base = {1, 48000};

    if (avcodec_open2(m_codecContext, codec, NULL) < 0) {
        LOG_ERROR("AudioEncoder", "Could not open Opus codec");
        return false;
    }

    m_swrContext = swr_alloc();
    av_opt_set_int(m_swrContext, "in_channel_layout", format.channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO, 0);
    av_opt_set_int(m_swrContext, "in_sample_rate", format.sampleRate, 0);
    av_opt_set_sample_fmt(m_swrContext, "in_sample_fmt", format.bitsPerSample == 32 ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16, 0);

    av_opt_set_int(m_swrContext, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(m_swrContext, "out_sample_rate", 48000, 0);
    av_opt_set_sample_fmt(m_swrContext, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

    if (swr_init(m_swrContext) < 0) {
        LOG_ERROR("AudioEncoder", "Could not initialize resampler");
        return false;
    }

    m_inputFrame = av_frame_alloc();
    m_inputFrame->nb_samples = m_codecContext->frame_size;
    m_inputFrame->format = m_codecContext->sample_fmt;
    m_inputFrame->channel_layout = m_codecContext->channel_layout;
    m_inputFrame->sample_rate = m_codecContext->sample_rate;
    if (av_frame_get_buffer(m_inputFrame, 0) < 0) return false;

    m_packet = av_packet_alloc();

    m_frameSize = m_codecContext->frame_size; // Usually 960 for 20ms

    return true;
}

void FFmpegAudioEncoder::Shutdown() {
    if (m_codecContext) { avcodec_free_context(&m_codecContext); m_codecContext = nullptr; }
    if (m_swrContext) { swr_free(&m_swrContext); m_swrContext = nullptr; }
    if (m_inputFrame) { av_frame_free(&m_inputFrame); m_inputFrame = nullptr; }
    if (m_packet) { av_packet_free(&m_packet); m_packet = nullptr; }
}

bool FFmpegAudioEncoder::Encode(const Audio::AudioFrame& pcmFrame, std::vector<uint8_t>& outEncoded) {
    // 1. Accumulate PCM data
    m_pcmBuffer.insert(m_pcmBuffer.end(), pcmFrame.data.begin(), pcmFrame.data.end());

    size_t bytesPerSample = (m_sourceFormat.bitsPerSample / 8) * m_sourceFormat.channels;
    size_t requiredBytes = m_frameSize * bytesPerSample;

    while (m_pcmBuffer.size() >= requiredBytes) {
        // 2. Resample to Opus format
        const uint8_t* inData = m_pcmBuffer.data();
        int delay = swr_get_delay(m_swrContext, m_sourceFormat.sampleRate);
        int outSamples = av_rescale_rnd(delay + m_frameSize, 48000, m_sourceFormat.sampleRate, AV_ROUND_UP);

        if (swr_convert(m_swrContext, m_inputFrame->data, m_frameSize, &inData, m_frameSize) < 0) {
            return false;
        }

        m_pcmBuffer.erase(m_pcmBuffer.begin(), m_pcmBuffer.begin() + requiredBytes);

        // 3. Encode
        m_inputFrame->pts = pcmFrame.timestamp; // Roughly

        int ret = avcodec_send_frame(m_codecContext, m_inputFrame);
        if (ret < 0) return false;

        while (ret >= 0) {
            ret = avcodec_receive_packet(m_codecContext, m_packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            else if (ret < 0) return false;

            outEncoded.insert(outEncoded.end(), m_packet->data, m_packet->data + m_packet->size);
            av_packet_unref(m_packet);
            return true; // Send one packet at a time for simplicity in this implementation
        }
    }

    return false;
}

} // namespace Host
