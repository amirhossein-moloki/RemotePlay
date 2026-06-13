#pragma once

#include <vector>
#include <string>

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
    bool Initialize(int width, int height, int fps, int bitrateKbps) override;
    bool EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets) override;
    void Shutdown() override;

private:
    int m_width, m_height;
    std::string m_encoderName;
};

} // namespace Host
