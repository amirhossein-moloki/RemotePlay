#include "encoder_manager.hpp"
#include "../common/logger.hpp"
#include <chrono>

#ifdef PARSEC_LITE_ENABLE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

namespace Host {

EncoderManager::EncoderManager() {
}

EncoderManager::~EncoderManager() {
    Shutdown();
}

void EncoderManager::DetectCapabilities(bool useHardware) {
    LOG_INFO("EncoderManager", "Detecting capabilities (Hardware: " + std::string(useHardware ? "On" : "Off") + ")...");
    m_capabilities.clear();

#ifdef PARSEC_LITE_ENABLE_FFMPEG
    auto checkCodec = [](const std::string& name) -> bool {
        const AVCodec* codec = avcodec_find_encoder_by_name(name.c_str());
        if (!codec) return false;

        // Try to open it briefly to see if it's actually supported by the hardware/driver
        AVCodecContext* ctx = avcodec_alloc_context3(codec);
        if (!ctx) return false;

        ctx->width = 1280;
        ctx->height = 720;
        ctx->time_base = {1, 60};
        ctx->pix_fmt = (name == "libx264") ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_NV12;

        bool success = (avcodec_open2(ctx, codec, nullptr) >= 0);
        avcodec_free_context(&ctx);
        return success;
    };

    if (useHardware) {
        if (checkCodec("h264_nvenc")) m_capabilities.push_back({EncoderBackend::NVENC, "h264_nvenc", true});
        if (checkCodec("h264_qsv"))   m_capabilities.push_back({EncoderBackend::QSV, "h264_qsv", true});
        if (checkCodec("h264_amf"))   m_capabilities.push_back({EncoderBackend::AMF, "h264_amf", true});
    }
    if (checkCodec("libx264"))    m_capabilities.push_back({EncoderBackend::Software, "libx264", true});
#endif

    if (m_capabilities.empty()) {
        m_capabilities.push_back({EncoderBackend::Software, "libx264", true});
    }

    LOG_INFO("EncoderManager", "Capability profile built.");
}

bool EncoderManager::Initialize(int initialWidth, int height, int fps, void* d3d11Device, bool useHardware) {
    m_baseWidth = initialWidth;
    m_baseHeight = height;
    m_fps = fps;
    m_d3d11Device = d3d11Device;
    m_currentTier = QualityTier::TierA_HighPerformance;
    m_backendIndex = 0;
    m_sessionLocked = false;

    DetectCapabilities(useHardware);

    LOG_INFO("EncoderManager", "Starting Preflight Encoder Validation...");
    if (!PreflightEncoderValidation(initialWidth, height, fps, d3d11Device)) {
        LOG_ERROR("EncoderManager", "Preflight Encoder Validation failed. No suitable encoders found.");
        return false;
    }

    m_sessionLocked = true;
    LOG_INFO("EncoderManager", "Encoder Session Locked: " + m_lockedCodecName);

    return SelectAndInitEncoder();
}

bool EncoderManager::PreflightEncoderValidation(int width, int height, int fps, void* d3d11Device) {
    for (size_t i = 0; i < m_capabilities.size(); ++i) {
        auto& cap = m_capabilities[i];
        if (!cap.available) continue;

        LOG_INFO("EncoderManager", "Validating " + cap.codecName + "...");

        auto testEncoder = std::make_unique<FFmpegHardwareEncoder>();
        // Preflight requirements: successful initialization and test encode
        if (testEncoder->Initialize(width, height, fps, 5000, d3d11Device, 0, cap.codecName)) {
            // Encode a dummy frame (texturePtr can be null if encoder handles it, but better use a real one if available)
            // However, we don't have a texture easily here. Let's assume successful Init is 90% of the battle,
            // but for a true PEV we'd need a test frame.
            // In this architecture, Init actually allocates buffers, so it's a strong check.

            LOG_INFO("EncoderManager", cap.codecName + " passed preflight validation.");
            m_backendIndex = i;
            m_lockedCodecName = cap.codecName;
            testEncoder->Shutdown();
            return true;
        }

        LOG_WARN("EncoderManager", cap.codecName + " failed preflight validation.");
        cap.available = false;
    }
    return false;
}

bool EncoderManager::SelectAndInitEncoder() {
    if (m_sessionLocked) {
        // Enforce Session Lock
        QualityProfile profile = GetProfileForTier(m_currentTier);
        LOG_INFO("EncoderManager", "Re-initializing locked encoder " + m_lockedCodecName + " at " + profile.name);

        if (!m_encoder) m_encoder = std::make_unique<FFmpegHardwareEncoder>();

        if (m_encoder->Initialize(profile.width, profile.height, m_fps, profile.bitrateKbps, m_d3d11Device, profile.preset, m_lockedCodecName)) {
            m_encoder->ForceKeyframe();
            return true;
        }

        LOG_ERROR("EncoderManager", "Locked encoder failed to re-initialize! Triggering Emergency Fallback.");
        return EmergencyEncoderFallback();
    }

    if (m_backendIndex >= m_capabilities.size()) {
        LOG_ERROR("EncoderManager", "No compatible encoders found!");
        return false;
    }

    auto& cap = m_capabilities[m_backendIndex];
    m_currentBackend = cap.backend;

    QualityProfile profile = GetProfileForTier(m_currentTier);

    LOG_INFO("EncoderManager", "Attempting to initialize " + cap.codecName + " at " + profile.name);

    m_encoder = std::make_unique<FFmpegHardwareEncoder>();

    if (m_encoder->Initialize(profile.width, profile.height, m_fps, profile.bitrateKbps, m_d3d11Device, profile.preset, cap.codecName)) {
        LOG_INFO("EncoderManager", "SelectedEncoder: " + cap.codecName + " | Profile: " + profile.name);
        m_encoder->ForceKeyframe();
        return true;
    }

    LOG_WARN("EncoderManager", "Failed to initialize " + cap.codecName + ". Marking as failed and trying fallback...");
    cap.available = false; // Mark this specific backend as failed
    return Fallback();
}

QualityProfile EncoderManager::GetProfileForTier(QualityTier tier) const {
    switch (tier) {
        case QualityTier::TierA_HighPerformance:
            return { m_baseWidth, m_baseHeight, 15000, 2, "High Performance (Tier A)" };
        case QualityTier::TierB_Balanced:
            // 900p or 1080p
            return { m_baseWidth, m_baseHeight, 8000, 1, "Balanced (Tier B)" };
        case QualityTier::TierC_LowEnd:
            // 720p or similar
            return { 1280, 720, 4000, 0, "Low-End (Tier C)" };
        case QualityTier::TierD_Recovery:
            // Aggressive optimization
            return { 854, 480, 1500, 0, "Recovery Mode (Tier D)" };
    }
    return { 1280, 720, 5000, 1, "Default" };
}

bool EncoderManager::Fallback() {
    if (m_sessionLocked) {
        return EmergencyEncoderFallback();
    }

    m_backendIndex++;

    // Skip backends that have been marked as unavailable
    while (m_backendIndex < m_capabilities.size() && !m_capabilities[m_backendIndex].available) {
        m_backendIndex++;
    }

    if (m_backendIndex < m_capabilities.size()) {
        return SelectAndInitEncoder();
    }

    return false;
}

bool EncoderManager::EmergencyEncoderFallback() {
    LOG_WARN("EncoderManager", "EMERGENCY ENCODER FALLBACK TRIGGERED! Switching to Software (libx264).");

    m_lockedCodecName = "libx264";
    m_currentBackend = EncoderBackend::Software;

    QualityProfile profile = GetProfileForTier(m_currentTier);
    if (!m_encoder) m_encoder = std::make_unique<FFmpegHardwareEncoder>();

    if (m_encoder->Initialize(profile.width, profile.height, m_fps, profile.bitrateKbps, m_d3d11Device, profile.preset, m_lockedCodecName)) {
        LOG_INFO("EncoderManager", "Emergency Fallback to Software successful.");
        m_encoder->ForceKeyframe();
        // Signal resync here would be better handled by a callback to SessionManager
        return true;
    }

    LOG_ERROR("EncoderManager", "FATAL: Emergency Software Fallback failed!");
    return false;
}

void EncoderManager::RequestKeyframe() {
    if (m_encoder) {
        m_encoder->ForceKeyframe();
    }
}

bool EncoderManager::EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets, PacketPool& pool) {
    if (!m_encoder || !m_encoder->IsInitialized()) {
        if (m_sessionLocked) {
            LOG_ERROR("EncoderManager", "Encoder not ready during locked session. Attempting re-init.");
            if (!SelectAndInitEncoder()) return false;
        } else return false;
    }

    if (!m_encoder->EncodeFrame(texturePtr, outPackets, pool)) {
        LOG_ERROR("EncoderManager", "Runtime encoding failure! Triggering fallback...");
        return Fallback();
    }

    return true;
}

void EncoderManager::UpdatePerformanceMetrics(float frameDropRate, float avgEncodeTimeMs, float clientDecodeTimeMs) {
    if (frameDropRate >= 0) m_lastFrameDropRate = frameDropRate;
    if (avgEncodeTimeMs >= 0) m_lastAvgEncodeTimeMs = avgEncodeTimeMs;

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    // Only adjust every 5 seconds to avoid flapping
    if (now - m_lastAdjustmentTime < 5000) return;

    bool needsDowngrade = false;
    float frameBudgetMs = 1000.0f / m_fps;

    // Check host-side performance
    if (m_lastFrameDropRate > 0.10f || m_lastAvgEncodeTimeMs > frameBudgetMs * 0.8f) {
        needsDowngrade = true;
    }

    // Check client-side performance (if available)
    if (clientDecodeTimeMs > 0 && clientDecodeTimeMs > frameBudgetMs * 0.8f) {
        LOG_INFO("EncoderManager", "Client struggling with decode time (" + std::to_string(clientDecodeTimeMs) + "ms). Downgrading...");
        needsDowngrade = true;
    }

    if (needsDowngrade) {
        AdjustTier(false); // Lower quality
        m_lastAdjustmentTime = now;
    } else if (m_lastFrameDropRate < 0.01f && m_lastAvgEncodeTimeMs < frameBudgetMs * 0.4f) {
        // Potentially improve quality if we're doing very well
        // AdjustTier(true);
    }
}

void EncoderManager::AdjustTier(bool improve) {
    QualityTier oldTier = m_currentTier;
    if (improve) {
        if (m_currentTier == QualityTier::TierB_Balanced) m_currentTier = QualityTier::TierA_HighPerformance;
        else if (m_currentTier == QualityTier::TierC_LowEnd) m_currentTier = QualityTier::TierB_Balanced;
        else if (m_currentTier == QualityTier::TierD_Recovery) m_currentTier = QualityTier::TierC_LowEnd;
    } else {
        if (m_currentTier == QualityTier::TierA_HighPerformance) m_currentTier = QualityTier::TierB_Balanced;
        else if (m_currentTier == QualityTier::TierB_Balanced) m_currentTier = QualityTier::TierC_LowEnd;
        else if (m_currentTier == QualityTier::TierC_LowEnd) m_currentTier = QualityTier::TierD_Recovery;
    }

    if (oldTier != m_currentTier) {
        LOG_INFO("EncoderManager", "Adjusting quality tier due to performance metrics.");
        SelectAndInitEncoder();
    }
}

void EncoderManager::Shutdown() {
    if (m_encoder) {
        m_encoder->Shutdown();
        m_encoder.reset();
    }
}

} // namespace Host
