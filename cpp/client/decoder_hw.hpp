#pragma once

#include <cstdint>
#include <cstddef>

namespace Client {

class DecoderHW {
public:
    DecoderHW();
    ~DecoderHW();

    bool Initialize();
    bool DecodeFrame(const uint8_t* data, size_t size, void** outTexture);
    void Shutdown();
};

} // namespace Client
