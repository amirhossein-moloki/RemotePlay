#pragma once

#include "common/audio_interfaces.hpp"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>
#include <atomic>
#include <mutex>

namespace Host {

class AudioCaptureWASAPI : public Audio::IAudioCapture {
public:
    AudioCaptureWASAPI();
    virtual ~AudioCaptureWASAPI();

    bool Initialize() override;
    void Shutdown() override;
    bool Start(std::function<void(const Audio::AudioFrame&)> callback) override;
    void Stop() override;

private:
    void CaptureThread();

    std::atomic<bool> m_running{false};
    std::thread m_thread;
    std::function<void(const Audio::AudioFrame&)> m_callback;

    IAudioClient* m_audioClient = nullptr;
    IAudioCaptureClient* m_captureClient = nullptr;
    WAVEFORMATEX* m_waveFormat = nullptr;
    HANDLE m_event = nullptr;
};

} // namespace Host
