#pragma once

#include <vector>
#include <cstdint>
#include <memory>

namespace Host {

class AudioEncoder {
public:
    AudioEncoder();
    ~AudioEncoder();

    bool Initialize(int sampleRate, int channels, int bitrate);
    bool Encode(const float* pcmData, size_t samples, std::vector<uint8_t>& outOpusData);
    void Shutdown();

private:
    struct InternalData;
    InternalData* m_internal = nullptr;
};

} // namespace Host
