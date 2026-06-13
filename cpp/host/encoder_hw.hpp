#pragma once

#include <vector>
#include <string>
#include <cstdint>

#ifdef _WIN32
#include <d3d11.h>
#endif

namespace Host {

struct EncodedPacket {
    std::vector<uint8_t> data;
    bool isKeyframe;
    uint64_t timestamp;
};

class EncoderHW {
public:
    virtual ~EncoderHW() {}
    virtual bool Initialize(int width, int height, int fps, int bitrateKbps) = 0;
    virtual bool EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets) = 0;
    virtual void Shutdown() = 0;
};

class FFmpegHardwareEncoder : public EncoderHW {
public:
    FFmpegHardwareEncoder();
    ~FFmpegHardwareEncoder() override;

    bool Initialize(int width, int height, int fps, int bitrateKbps) override;
    bool EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets) override;
    void Shutdown() override;

private:
    bool SetupHardwareContext();

    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    int m_bitrate = 0;

    struct InternalData;
    InternalData* m_internal = nullptr;
};

} // namespace Host
