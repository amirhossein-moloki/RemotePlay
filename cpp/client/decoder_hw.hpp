#pragma once

#include <cstdint>
#include <vector>

#ifdef _WIN32
#include <d3d11.h>
#endif

namespace Client {

class DecoderHW {
public:
    DecoderHW();
    ~DecoderHW();

    bool Initialize(void* d3d11DevicePtr = nullptr);
    bool DecodeFrame(const uint8_t* data, size_t size, void** outTexture);
    bool IsHardware() const;
    void Shutdown();

private:
    struct InternalData;
    InternalData* m_internal = nullptr;
};

} // namespace Client
