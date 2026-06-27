#include "audio_renderer_wasapi.hpp"
#include "common/logger.hpp"

namespace Client {

AudioRendererWASAPI::AudioRendererWASAPI() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

AudioRendererWASAPI::~AudioRendererWASAPI() {
    Shutdown();
    CoUninitialize();
}

bool AudioRendererWASAPI::Initialize(const Audio::AudioFormat& format) {
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

    // Set up format - ideally we use what the decoder produces (48kHz, Stereo, Float or S16)
    WAVEFORMATEXTENSIBLE wfx = {};
    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.nChannels = format.channels;
    wfx.Format.nSamplesPerSec = format.sampleRate;
    wfx.Format.wBitsPerSample = format.bitsPerSample;
    wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
    wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
    wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfx.Samples.wValidBitsPerSample = format.bitsPerSample;
    wfx.dwChannelMask = (format.channels == 2) ? KSAUDIO_SPEAKER_STEREO : KSAUDIO_SPEAKER_MONO;
    wfx.SubFormat = (format.bitsPerSample == 32) ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;

    // Use 20ms buffer for low latency (200,000 hns)
    REFERENCE_TIME bufferDuration = 200000;
    hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, (WAVEFORMATEX*)&wfx, NULL);
    if (FAILED(hr)) {
        LOG_ERROR("AudioRenderer", "Failed to initialize WASAPI Render Client. Trying mix format.");
        // Fallback to mix format if specific format fails
        hr = m_audioClient->GetMixFormat(&m_waveFormat);
        if (FAILED(hr)) return false;
        hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, m_waveFormat, NULL);
        if (FAILED(hr)) return false;
    }

    hr = m_audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_renderClient);
    if (FAILED(hr)) return false;

    hr = m_audioClient->GetService(__uuidof(ISimpleAudioVolume), (void**)&m_audioVolume);
    if (FAILED(hr)) return false;

    m_audioClient->Start();

    return true;
}

void AudioRendererWASAPI::Shutdown() {
    if (m_audioClient) { m_audioClient->Stop(); }
    if (m_renderClient) { m_renderClient->Release(); m_renderClient = nullptr; }
    if (m_audioVolume) { m_audioVolume->Release(); m_audioVolume = nullptr; }
    if (m_audioClient) { m_audioClient->Release(); m_audioClient = nullptr; }
    if (m_waveFormat) { CoTaskMemFree(m_waveFormat); m_waveFormat = nullptr; }
}

void AudioRendererWASAPI::Play(const Audio::AudioFrame& pcmFrame) {
    if (!m_renderClient) return;

    UINT32 numSamples = (UINT32)(pcmFrame.data.size() / ((pcmFrame.format.bitsPerSample / 8) * pcmFrame.format.channels));

    BYTE* data = nullptr;
    HRESULT hr = m_renderClient->GetBuffer(numSamples, &data);
    if (SUCCEEDED(hr)) {
        memcpy(data, pcmFrame.data.data(), pcmFrame.data.size());
        m_renderClient->ReleaseBuffer(numSamples, 0);
    }
}

void AudioRendererWASAPI::SetVolume(float volume) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_volume = volume;
    if (m_audioVolume) m_audioVolume->SetMasterVolume(m_volume, NULL);
}

void AudioRendererWASAPI::SetMute(bool mute) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mute = mute;
    if (m_audioVolume) m_audioVolume->SetMute(m_mute, NULL);
}

} // namespace Client
