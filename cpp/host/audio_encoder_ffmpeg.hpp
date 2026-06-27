#pragma once

#include "common/audio_interfaces.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace Host {

class FFmpegAudioEncoder : public Audio::IAudioEncoder {
public:
    FFmpegAudioEncoder();
    virtual ~FFmpegAudioEncoder();

    bool Initialize(const Audio::AudioFormat& format, int bitrateKbps) override;
    void Shutdown() override;
    bool Encode(const Audio::AudioFrame& pcmFrame, std::vector<uint8_t>& outEncoded) override;

private:
    AVCodecContext* m_codecContext = nullptr;
    SwrContext* m_swrContext = nullptr;
    AVFrame* m_inputFrame = nullptr;
    AVPacket* m_packet = nullptr;

    Audio::AudioFormat m_sourceFormat;
    int m_frameSize = 960; // 20ms at 48kHz
    std::vector<uint8_t> m_pcmBuffer;
};

} // namespace Host
