#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <atomic>
#include "../common/packet_pool.hpp"

#ifdef _WIN32
#include <d3d11.h>
#endif

namespace Host {

struct EncodedPacket {
    std::unique_ptr<PacketPool::Packet> packet;
    bool isKeyframe;
    bool isHEVC = false;
    uint64_t captureTimestamp;
    uint64_t encodeStartTimestamp;
    uint64_t encodeEndTimestamp;
};

class EncoderHW {
public:
    virtual ~EncoderHW() {}
    virtual bool Initialize(int width, int height, int fps, int bitrateKbps, void* d3d11Device = nullptr, int preset = 0, const std::string& codecName = "") = 0;
    virtual bool EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets, PacketPool& pool) = 0;
    virtual void SetBitrate(int bitrateKbps) = 0;
    virtual void ForceKeyframe() = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;
    virtual bool IsHEVC() const = 0;
};

class FFmpegHardwareEncoder : public EncoderHW {
public:
    FFmpegHardwareEncoder();
    ~FFmpegHardwareEncoder() override;

    bool Initialize(int width, int height, int fps, int bitrateKbps, void* d3d11Device = nullptr, int preset = 0, const std::string& codecName = "") override;
    bool EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets, PacketPool& pool) override;
    void SetBitrate(int bitrateKbps) override;
    void ForceKeyframe() override;
    void Shutdown() override;
    bool IsInitialized() const override { return m_initialized; }
    bool IsHEVC() const override;

private:
    bool SetupHardwareContext();

    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    int m_bitrate = 0;
    bool m_initialized = false;
    std::atomic<bool> m_forceKeyframe{false};

    struct InternalData;
    InternalData* m_internal = nullptr;
};

} // namespace Host
