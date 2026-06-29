#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "encoder_hw.hpp"

namespace Host {

enum class StreamingState {
    IDLE,
    PREFLIGHT_VALIDATION,
    READY,
    STARTING,
    STREAMING,
    DEGRADED,
    RECOVERING,
    EMERGENCY_FALLBACK,
    SHUTDOWN
};

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

    bool Initialize(int initialWidth, int height, int fps, void* d3d11Device, bool useHardware = true);
    bool EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets, PacketPool& pool);

    void RequestKeyframe();
    void UpdatePerformanceMetrics(float frameDropRate, float avgEncodeTimeMs, float clientDecodeTimeMs = -1.0f);

    QualityTier GetCurrentTier() const { return m_currentTier; }
    EncoderBackend GetCurrentBackend() const { return m_currentBackend; }
    int GetCurrentBitrate() const { return GetProfileForTier(m_currentTier).bitrateKbps; }
    std::string GetLockedCodecName() const { return m_lockedCodecName; }
    bool IsHEVC() const;

    void Shutdown();

    StreamingState GetState() const { return m_state; }

private:
    void SetState(StreamingState newState);
    void DetectCapabilities(bool useHardware);
    bool PreflightEncoderValidation(int width, int height, int fps, void* d3d11Device);
    bool SelectAndInitEncoder();
    QualityProfile GetProfileForTier(QualityTier tier) const;

    bool Fallback();
    bool EmergencyEncoderFallback();
    void AdjustTier(bool improve);

    std::vector<EncoderCapability> m_capabilities;
    std::unique_ptr<EncoderHW> m_encoder;
    mutable std::mutex m_encoderMutex;

    StreamingState m_state = StreamingState::IDLE;
    QualityTier m_currentTier = QualityTier::TierA_HighPerformance;
    EncoderBackend m_currentBackend = EncoderBackend::None;
    size_t m_backendIndex = 0; // Current index in prioritized capabilities
    bool m_sessionLocked = false;
    std::string m_lockedCodecName;

    int m_baseWidth = 1920;
    int m_baseHeight = 1080;
    int m_fps = 60;
    void* m_d3d11Device = nullptr;

    float m_lastFrameDropRate = 0.0f;
    float m_lastAvgEncodeTimeMs = 0.0f;
    uint64_t m_lastAdjustmentTime = 0;
};

} // namespace Host
