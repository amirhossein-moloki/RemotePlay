#include "audio_capture.hpp"
#include "../common/logger.hpp"
#include <avrt.h>

#ifdef _WIN32
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "Ole32.lib")

namespace Host {

AudioCapture::AudioCapture() {}

AudioCapture::~AudioCapture() {
    Stop();
}

bool AudioCapture::Initialize(std::function<void(const float*, size_t)> callback) {
    m_callback = callback;
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&m_deviceEnumerator);
    if (FAILED(hr)) return false;

    hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
    if (FAILED(hr)) return false;

    hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audioClient);
    if (FAILED(hr)) return false;

    hr = m_audioClient->GetMixFormat(&m_pwfx);
    if (FAILED(hr)) return false;

    // Ensure we get float format if possible, or we'd need a resampler
    if (m_pwfx->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
        // Fallback or error
    }

    hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, m_pwfx, NULL);
    if (FAILED(hr)) return false;

    m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    hr = m_audioClient->SetEventHandle(m_event);
    if (FAILED(hr)) return false;

    hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureClient);
    if (FAILED(hr)) return false;

    return true;
}

void AudioCapture::Start() {
    if (m_running) return;
    m_running = true;
    m_audioClient->Start();
    m_thread = std::thread(&AudioCapture::CaptureThread, this);
}

void AudioCapture::Stop() {
    m_running = false;
    if (m_event) SetEvent(m_event);
    if (m_thread.joinable()) m_thread.join();
    if (m_audioClient) m_audioClient->Stop();

    if (m_captureClient) m_captureClient->Release();
    if (m_audioClient) m_audioClient->Release();
    if (m_device) m_device->Release();
    if (m_deviceEnumerator) m_deviceEnumerator->Release();
    if (m_pwfx) CoTaskMemFree(m_pwfx);

    m_captureClient = nullptr;
    m_audioClient = nullptr;
    m_device = nullptr;
    m_deviceEnumerator = nullptr;
    m_pwfx = nullptr;
}

void AudioCapture::CaptureThread() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    DWORD taskIndex = 0;
    HANDLE avrtHandle = AvSetMmThreadCharacteristicsW(L"Audio", &taskIndex);

    while (m_running) {
        WaitForSingleObject(m_event, INFINITE);
        if (!m_running) break;

        UINT32 nextPacketSize = 0;
        HRESULT hr = m_captureClient->GetNextPacketSize(&nextPacketSize);
        while (SUCCEEDED(hr) && nextPacketSize > 0) {
            BYTE* pData;
            UINT32 numFramesRead;
            DWORD flags;
            hr = m_captureClient->GetBuffer(&pData, &numFramesRead, &flags, NULL, NULL);
            if (SUCCEEDED(hr)) {
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    // Assuming m_pwfx is float (standard for mix format)
                    m_callback((const float*)pData, (size_t)numFramesRead * m_pwfx->nChannels);
                }
                m_captureClient->ReleaseBuffer(numFramesRead);
            }
            hr = m_captureClient->GetNextPacketSize(&nextPacketSize);
        }
    }

    if (avrtHandle) AvRevertMmThreadCharacteristics(avrtHandle);
}

} // namespace Host
#endif
