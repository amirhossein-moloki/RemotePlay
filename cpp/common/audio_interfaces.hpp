#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <string>

namespace Audio {

struct AudioFormat {
    uint32_t sampleRate = 48000;
    uint32_t channels = 2;
    uint32_t bitsPerSample = 16; // We'll use float or int16
};

struct AudioFrame {
    std::vector<uint8_t> data;
    uint64_t timestamp; // microseconds
    AudioFormat format;
};

// Interface for Audio Capture (Host side)
class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual bool Start(std::function<void(const AudioFrame&)> callback) = 0;
    virtual void Stop() = 0;
};

// Interface for Audio Encoder (Host side)
class IAudioEncoder {
public:
    virtual ~IAudioEncoder() = default;
    virtual bool Initialize(const AudioFormat& format, int bitrateKbps) = 0;
    virtual void Shutdown() = 0;
    virtual bool Encode(const AudioFrame& pcmFrame, std::vector<uint8_t>& outEncoded) = 0;
};

// Interface for Audio Decoder (Client side)
class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;
    virtual bool Initialize(const AudioFormat& format) = 0;
    virtual void Shutdown() = 0;
    virtual bool Decode(const uint8_t* encodedData, size_t size, AudioFrame& outPcmFrame) = 0;
};

// Interface for Audio Renderer (Client side)
class IAudioRenderer {
public:
    virtual ~IAudioRenderer() = default;
    virtual bool Initialize(const AudioFormat& format) = 0;
    virtual void Shutdown() = 0;
    virtual void Play(const AudioFrame& pcmFrame) = 0;
    virtual void SetVolume(float volume) = 0; // 0.0 to 1.0
    virtual void SetMute(bool mute) = 0;
};

} // namespace Audio
