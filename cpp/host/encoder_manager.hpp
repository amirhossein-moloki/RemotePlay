#pragma once

#include <string>
#include <vector>
#include <memory>
#include "encoder_hw.hpp"

namespace Host {

enum class QualityTier {
    TierA_HighPerformance,
    TierB_Balanced,
    TierC_LowEnd,
    TierD_Recovery
};

struct QualityProfile {
    int width;
    int height;
    int bitrateKbps;
    int preset; // 0: Performance, 1: Balanced, 2: Quality
    std::string name;
};

enum class EncoderBackend {
    NVENC,
    AMF,
    QSV,
    Software,
    None
};

struct EncoderCapability {
    EncoderBackend backend;
    std::string codecName;
    bool available;
};

class EncoderManager {
public:
    EncoderManager();
    ~EncoderManager();

    bool Initialize(int initialWidth, int height, int fps, void* d3d11Device);
    bool EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets, PacketPool& pool);

    void UpdatePerformanceMetrics(float frameDropRate, float avgEncodeTimeMs, float clientDecodeTimeMs = -1.0f);

    QualityTier GetCurrentTier() const { return m_currentTier; }
    EncoderBackend GetCurrentBackend() const { return m_currentBackend; }
    int GetCurrentBitrate() const { return GetProfileForTier(m_currentTier).bitrateKbps; }

    void Shutdown();

private:
    void DetectCapabilities();
    bool SelectAndInitEncoder();
    QualityProfile GetProfileForTier(QualityTier tier) const;

    bool Fallback();
    void AdjustTier(bool improve);

    std::vector<EncoderCapability> m_capabilities;
    std::unique_ptr<EncoderHW> m_encoder;

    QualityTier m_currentTier = QualityTier::TierA_HighPerformance;
    EncoderBackend m_currentBackend = EncoderBackend::None;
    size_t m_backendIndex = 0; // Current index in prioritized capabilities

    int m_baseWidth = 1920;
    int m_baseHeight = 1080;
    int m_fps = 60;
    void* m_d3d11Device = nullptr;

    float m_lastFrameDropRate = 0.0f;
    float m_lastAvgEncodeTimeMs = 0.0f;
    uint64_t m_lastAdjustmentTime = 0;
};

} // namespace Host
