#pragma once

#include "common/audio_interfaces.hpp"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mutex>

namespace Client {

class AudioRendererWASAPI : public Audio::IAudioRenderer {
public:
    AudioRendererWASAPI();
    virtual ~AudioRendererWASAPI();

    bool Initialize(const Audio::AudioFormat& format) override;
    void Shutdown() override;
    void Play(const Audio::AudioFrame& pcmFrame) override;
    void SetVolume(float volume) override;
    void SetMute(bool mute) override;

private:
    IAudioClient* m_audioClient = nullptr;
    IAudioRenderClient* m_renderClient = nullptr;
    ISimpleAudioVolume* m_audioVolume = nullptr;
    WAVEFORMATEX* m_waveFormat = nullptr;

    std::mutex m_mutex;
    float m_volume = 1.0f;
    bool m_mute = false;
};

} // namespace Client
