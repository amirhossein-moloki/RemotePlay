#pragma once

#include <vector>
#include <cstdint>

namespace Client {

class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();

    bool Initialize(int sampleRate, int channels);
    bool Decode(const uint8_t* opusData, size_t size, std::vector<float>& outPcmData);
    void Shutdown();

private:
    struct InternalData;
    InternalData* m_internal = nullptr;
};

} // namespace Client
