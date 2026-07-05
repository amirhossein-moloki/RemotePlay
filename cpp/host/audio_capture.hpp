#pragma once

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>

namespace Host {

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    bool Initialize(std::function<void(const float*, size_t)> callback);
    void Start();
    void Stop();

private:
    void CaptureThread();

    std::function<void(const float*, size_t)> m_callback;
    std::atomic<bool> m_running{false};
    std::thread m_thread;

    IMMDeviceEnumerator* m_deviceEnumerator = nullptr;
    IMMDevice* m_device = nullptr;
    IAudioClient* m_audioClient = nullptr;
    IAudioCaptureClient* m_captureClient = nullptr;
    WAVEFORMATEX* m_pwfx = nullptr;
    HANDLE m_event = NULL;
};

} // namespace Host
#endif
