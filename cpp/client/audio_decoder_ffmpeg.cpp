#include "audio_decoder_ffmpeg.hpp"
#include "common/logger.hpp"

namespace Client {

FFmpegAudioDecoder::FFmpegAudioDecoder() {}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
    Shutdown();
}

bool FFmpegAudioDecoder::Initialize(const Audio::AudioFormat& format) {
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    if (!codec) return false;

    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) return false;

    m_codecContext->sample_rate = 48000;
    m_codecContext->ch_layout = AV_CHANNEL_LAYOUT_STEREO;

    if (avcodec_open2(m_codecContext, codec, NULL) < 0) return false;

    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();

    m_swrContext = swr_alloc();
    // Default Opus output is 48kHz Stereo Float Planar.
    // We want to convert to S16 Interleaved for common WASAPI support.
    av_opt_set_chlayout(m_swrContext, "in_chlayout", &m_codecContext->ch_layout, 0);
    av_opt_set_int(m_swrContext, "in_sample_rate", 48000, 0);
    av_opt_set_sample_fmt(m_swrContext, "in_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    av_opt_set_chlayout(m_swrContext, "out_chlayout", &out_ch_layout, 0);
    av_opt_set_int(m_swrContext, "out_sample_rate", 48000, 0);
    av_opt_set_sample_fmt(m_swrContext, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(m_swrContext) < 0) return false;

    return true;
}

void FFmpegAudioDecoder::Shutdown() {
    if (m_codecContext) { avcodec_free_context(&m_codecContext); m_codecContext = nullptr; }
    if (m_swrContext) { swr_free(&m_swrContext); m_swrContext = nullptr; }
    if (m_frame) { av_frame_free(&m_frame); m_frame = nullptr; }
    if (m_packet) { av_packet_free(&m_packet); m_packet = nullptr; }
}

bool FFmpegAudioDecoder::Decode(const uint8_t* encodedData, size_t size, Audio::AudioFrame& outPcmFrame) {
    m_packet->data = (uint8_t*)encodedData;
    m_packet->size = (int)size;

    int ret = avcodec_send_packet(m_codecContext, m_packet);
    if (ret < 0) return false;

    ret = avcodec_receive_frame(m_codecContext, m_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return false;
    else if (ret < 0) return false;

    // Resample to S16 Interleaved
    int outSamples = av_rescale_rnd(swr_get_delay(m_swrContext, 48000) + m_frame->nb_samples, 48000, 48000, AV_ROUND_UP);
    outPcmFrame.data.resize(outSamples * 2 * 2); // 2 channels, 2 bytes per sample

    uint8_t* outPtr = outPcmFrame.data.data();
    int converted = swr_convert(m_swrContext, &outPtr, outSamples, (const uint8_t**)m_frame->data, m_frame->nb_samples);
    if (converted < 0) return false;

    outPcmFrame.data.resize(converted * 2 * 2);
    outPcmFrame.timestamp = m_frame->pts;
    outPcmFrame.format.sampleRate = 48000;
    outPcmFrame.format.channels = 2;
    outPcmFrame.format.bitsPerSample = 16;

    return true;
}

} // namespace Client
