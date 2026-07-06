#include "audio_renderer.hpp"
#include "../common/logger.hpp"

#ifdef _WIN32
#include <ks.h>
#include <ksmedia.h>
#include <mmreg.h>
#pragma comment(lib, "Ole32.lib")

#ifndef SPEAKER_MONO
#define SPEAKER_MONO 0x00000004
#endif

namespace Client {

AudioRenderer::AudioRenderer() {}

AudioRenderer::~AudioRenderer() {
    Stop();
}

bool AudioRenderer::Initialize(int sampleRate, int channels) {
    m_channels = channels;
    HRESULT hr;
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) return false;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) return false;

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audioClient);
    if (FAILED(hr)) return false;

    WAVEFORMATEXTENSIBLE wfx = {};
    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.nChannels = channels;
    wfx.Format.nSamplesPerSec = sampleRate;
    wfx.Format.wBitsPerSample = 32;
    wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
    wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
    wfx.Format.cbSize = 22;
    wfx.Samples.wValidBitsPerSample = 32;
    wfx.dwChannelMask = (channels == 2) ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT) : SPEAKER_MONO;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, (WAVEFORMATEX*)&wfx, NULL);
    if (FAILED(hr)) return false;

    m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_audioClient->SetEventHandle(m_event);

    hr = m_audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_renderClient);
    if (FAILED(hr)) return false;

    enumerator->Release();
    device->Release();
    return true;
}

void AudioRenderer::PushSamples(const float* data, size_t samples) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.insert(m_buffer.end(), data, data + samples);
}

void AudioRenderer::Start() {
    if (m_running) return;
    m_running = true;
    m_audioClient->Start();
    m_thread = std::thread(&AudioRenderer::RenderThread, this);
}

void AudioRenderer::Stop() {
    m_running = false;
    if (m_event) SetEvent(m_event);
    if (m_thread.joinable()) m_thread.join();
}

void AudioRenderer::RenderThread() {
    while (m_running) {
        WaitForSingleObject(m_event, INFINITE);
        if (!m_running) break;

        UINT32 padding = 0;
        m_audioClient->GetCurrentPadding(&padding);
        UINT32 bufferSize = 0;
        m_audioClient->GetBufferSize(&bufferSize);
        UINT32 available = bufferSize - padding;

        if (available > 0) {
            BYTE* pData;
            m_renderClient->GetBuffer(available, &pData);

            std::lock_guard<std::mutex> lock(m_mutex);
            size_t toCopy = std::min((size_t)available * m_channels, m_buffer.size());
            if (toCopy > 0) {
                memcpy(pData, m_buffer.data(), toCopy * sizeof(float));
                m_buffer.erase(m_buffer.begin(), m_buffer.begin() + toCopy);
                m_renderClient->ReleaseBuffer((UINT32)(toCopy / m_channels), 0);
            } else {
                m_renderClient->ReleaseBuffer(available, AUDCLNT_BUFFERFLAGS_SILENT);
            }
        }
    }
}

} // namespace Client
#endif
