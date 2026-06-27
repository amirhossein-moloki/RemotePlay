#include "audio_capture_wasapi.hpp"
#include "common/logger.hpp"
#include <chrono>

namespace Host {

AudioCaptureWASAPI::AudioCaptureWASAPI() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

AudioCaptureWASAPI::~AudioCaptureWASAPI() {
    Shutdown();
    CoUninitialize();
}

bool AudioCaptureWASAPI::Initialize() {
    HRESULT hr;
    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* device = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);
    if (FAILED(hr)) return false;

    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    deviceEnumerator->Release();
    if (FAILED(hr)) return false;

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audioClient);
    device->Release();
    if (FAILED(hr)) return false;

    hr = m_audioClient->GetMixFormat(&m_waveFormat);
    if (FAILED(hr)) return false;

    // Adjust format to 48kHz Stereo if needed, but WASAPI Loopback usually requires MixFormat
    // For now, we'll use MixFormat and let the encoder handle it if possible,
    // or we might need to use Resampler later.

    hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, m_waveFormat, NULL);
    if (FAILED(hr)) return false;

    m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    hr = m_audioClient->SetEventHandle(m_event);
    if (FAILED(hr)) return false;

    hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureClient);
    if (FAILED(hr)) return false;

    LOG_INFO("AudioCapture", "WASAPI Loopback Initialized: " + std::to_string(m_waveFormat->nSamplesPerSec) + "Hz, " + std::to_string(m_waveFormat->nChannels) + " channels");

    return true;
}

void AudioCaptureWASAPI::Shutdown() {
    Stop();
    if (m_captureClient) { m_captureClient->Release(); m_captureClient = nullptr; }
    if (m_audioClient) { m_audioClient->Release(); m_audioClient = nullptr; }
    if (m_waveFormat) { CoTaskMemFree(m_waveFormat); m_waveFormat = nullptr; }
    if (m_event) { CloseHandle(m_event); m_event = nullptr; }
}

bool AudioCaptureWASAPI::Start(std::function<void(const Audio::AudioFrame&)> callback) {
    if (m_running) return true;
    m_callback = callback;
    m_running = true;
    m_thread = std::thread(&AudioCaptureWASAPI::CaptureThread, this);
    return true;
}

void AudioCaptureWASAPI::Stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void AudioCaptureWASAPI::CaptureThread() {
    HRESULT hr = m_audioClient->Start();
    if (FAILED(hr)) return;

    while (m_running) {
        WaitForSingleObject(m_event, 100);

        UINT32 nextPacketSize = 0;
        hr = m_captureClient->GetNextPacketSize(&nextPacketSize);

        while (nextPacketSize != 0 && m_running) {
            BYTE* data = nullptr;
            UINT32 numFramesRead = 0;
            DWORD flags = 0;
            UINT64 devicePosition = 0;
            UINT64 qpcPosition = 0;

            hr = m_captureClient->GetBuffer(&data, &numFramesRead, &flags, &devicePosition, &qpcPosition);
            if (SUCCEEDED(hr)) {
                Audio::AudioFrame frame;
                size_t bytesPerFrame = m_waveFormat->nBlockAlign;
                frame.data.assign(data, data + (numFramesRead * bytesPerFrame));

                // Convert QPC to microseconds
                static LARGE_INTEGER frequency;
                static bool freqInit = false;
                if (!freqInit) {
                    QueryPerformanceFrequency(&frequency);
                    freqInit = true;
                }
                frame.timestamp = (qpcPosition * 1000000) / (UINT64)frequency.QuadPart;

                frame.format.sampleRate = m_waveFormat->nSamplesPerSec;
                frame.format.channels = m_waveFormat->nChannels;
                frame.format.bitsPerSample = m_waveFormat->wBitsPerSample;

                if (m_callback) m_callback(frame);

                m_captureClient->ReleaseBuffer(numFramesRead);
            }

            hr = m_captureClient->GetNextPacketSize(&nextPacketSize);
        }
    }

    m_audioClient->Stop();
}

} // namespace Host
