#pragma once

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <atomic>
#include <mutex>

namespace Client {

class AudioRenderer {
public:
    AudioRenderer();
    ~AudioRenderer();

    bool Initialize(int sampleRate, int channels);
    void PushSamples(const float* data, size_t samples);
    void Start();
    void Stop();

private:
    void RenderThread();

    std::vector<float> m_buffer;
    std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    std::thread m_thread;

    IAudioClient* m_audioClient = nullptr;
    IAudioRenderClient* m_renderClient = nullptr;
    WAVEFORMATEX* m_pwfx = nullptr;
    HANDLE m_event = NULL;
    int m_channels = 2;
};

} // namespace Client
#endif
