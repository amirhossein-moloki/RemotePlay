#pragma once

#include "common/audio_interfaces.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

namespace Client {

class FFmpegAudioDecoder : public Audio::IAudioDecoder {
public:
    FFmpegAudioDecoder();
    virtual ~FFmpegAudioDecoder();

    bool Initialize(const Audio::AudioFormat& format) override;
    void Shutdown() override;
    bool Decode(const uint8_t* encodedData, size_t size, Audio::AudioFrame& outPcmFrame) override;

private:
    AVCodecContext* m_codecContext = nullptr;
    SwrContext* m_swrContext = nullptr;
    AVFrame* m_frame = nullptr;
    AVPacket* m_packet = nullptr;
};

} // namespace Client
