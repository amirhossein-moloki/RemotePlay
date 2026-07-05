#include "encoder_manager.hpp"
#include "../common/logger.hpp"
#include <chrono>

#ifdef PARSEC_LITE_ENABLE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>
}
#endif

namespace Host {

EncoderManager::EncoderManager() {
    SetState(StreamingState::IDLE);
    m_streamEngine = std::make_unique<AI::StreamEngine>();
}

EncoderManager::~EncoderManager() {
    Shutdown();
}

void EncoderManager::SetState(StreamingState newState) {
    if (m_state == newState) return;

    bool valid = false;
    switch (m_state) {
        case StreamingState::IDLE: valid = (newState == StreamingState::STARTING); break;
        case StreamingState::STARTING: valid = (newState == StreamingState::PREFLIGHT_VALIDATION || newState == StreamingState::SHUTDOWN); break;
        case StreamingState::PREFLIGHT_VALIDATION: valid = (newState == StreamingState::READY || newState == StreamingState::SHUTDOWN); break;
        case StreamingState::READY: valid = (newState == StreamingState::STREAMING || newState == StreamingState::SHUTDOWN); break;
        case StreamingState::STREAMING: valid = (newState == StreamingState::DEGRADED || newState == StreamingState::RECOVERING || newState == StreamingState::EMERGENCY_FALLBACK || newState == StreamingState::SHUTDOWN); break;
        case StreamingState::DEGRADED: valid = (newState == StreamingState::STREAMING || newState == StreamingState::RECOVERING || newState == StreamingState::EMERGENCY_FALLBACK || newState == StreamingState::SHUTDOWN); break;
        case StreamingState::RECOVERING: valid = (newState == StreamingState::STREAMING || newState == StreamingState::EMERGENCY_FALLBACK || newState == StreamingState::SHUTDOWN); break;
        case StreamingState::EMERGENCY_FALLBACK: valid = (newState == StreamingState::STREAMING || newState == StreamingState::SHUTDOWN); break;
        case StreamingState::SHUTDOWN: valid = (newState == StreamingState::IDLE); break;
    }

    const char* stateNames[] = { "IDLE", "PREFLIGHT_VALIDATION", "READY", "STARTING", "STREAMING", "DEGRADED", "RECOVERING", "EMERGENCY_FALLBACK", "SHUTDOWN" };
    if (valid) {
        LOG_INFO("EncoderManager", "State transition: " + std::string(stateNames[(int)m_state]) + " -> " + std::string(stateNames[(int)newState]));
        m_state = newState;
    } else {
        LOG_ERROR("EncoderManager", "INVALID State transition: " + std::string(stateNames[(int)m_state]) + " -> " + std::string(stateNames[(int)newState]));
    }
}

void EncoderManager::DetectCapabilities(bool useHardware) {
    LOG_INFO("EncoderManager", "Detecting capabilities...");
    m_capabilities.clear();

#ifdef PARSEC_LITE_ENABLE_FFMPEG
    auto checkCodec = [](const std::string& name) -> bool {
        const AVCodec* codec = avcodec_find_encoder_by_name(name.c_str());
        if (!codec) return false;

        AVCodecContext* ctx = avcodec_alloc_context3(codec);
        if (!ctx) return false;

        ctx->width = 1280; ctx->height = 720; ctx->time_base = {1, 60};
        ctx->pix_fmt = (name == "libx264") ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_NV12;

        const enum AVPixelFormat* pix_fmts = codec->pix_fmts;
        if (pix_fmts) {
            bool found = false;
            for (const enum AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
                if (*p == ctx->pix_fmt) { found = true; break; }
            }
            if (!found && pix_fmts[0] != AV_PIX_FMT_NONE) ctx->pix_fmt = pix_fmts[0];
        }

        bool success = (avcodec_open2(ctx, codec, nullptr) >= 0);
        avcodec_free_context(&ctx);
        return success;
    };

    if (useHardware) {
        const std::vector<std::pair<std::string, EncoderBackend>> hardwareCodecs = {
            {"hevc_nvenc", EncoderBackend::NVENC}, {"h264_nvenc", EncoderBackend::NVENC},
            {"hevc_qsv", EncoderBackend::QSV}, {"h264_qsv", EncoderBackend::QSV},
            {"hevc_amf", EncoderBackend::AMF}, {"h264_amf", EncoderBackend::AMF}
        };
        for (const auto& codecInfo : hardwareCodecs) {
            if (checkCodec(codecInfo.first)) m_capabilities.push_back({codecInfo.second, codecInfo.first, true});
        }
    }
    if (checkCodec("libx264")) m_capabilities.push_back({EncoderBackend::Software, "libx264", true});
#endif
    if (m_capabilities.empty()) m_capabilities.push_back({EncoderBackend::Software, "libx264", true});
}

bool EncoderManager::IsHEVC() const {
    if (m_encoder) return m_encoder->IsHEVC();
    return m_lockedCodecName.find("hevc") != std::string::npos || m_lockedCodecName.find("265") != std::string::npos;
}

bool EncoderManager::Initialize(int initialWidth, int height, int fps, void* d3d11Device, bool useHardware) {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    SetState(StreamingState::STARTING);
    m_baseWidth = initialWidth; m_baseHeight = height; m_fps = fps; m_d3d11Device = d3d11Device;
    m_currentTier = QualityTier::TierA_HighPerformance; m_backendIndex = 0; m_sessionLocked = false;
    DetectCapabilities(useHardware);
    SetState(StreamingState::PREFLIGHT_VALIDATION);
    if (!PreflightEncoderValidation(initialWidth, height, fps, d3d11Device)) {
        SetState(StreamingState::SHUTDOWN); return false;
    }
    m_sessionLocked = true;
    SetState(StreamingState::READY);
    if (SelectAndInitEncoder()) { SetState(StreamingState::STREAMING); return true; }
    SetState(StreamingState::SHUTDOWN); return false;
}

bool EncoderManager::PreflightEncoderValidation(int width, int height, int fps, void* d3d11Device) {
    for (size_t i = 0; i < m_capabilities.size(); ++i) {
        auto& cap = m_capabilities[i]; if (!cap.available) continue;
        auto testEncoder = std::make_unique<FFmpegHardwareEncoder>();
        if (testEncoder->Initialize(width, height, fps, 5000, d3d11Device, 0, cap.codecName)) {
            m_backendIndex = i; m_lockedCodecName = cap.codecName;
            testEncoder->Shutdown(); return true;
        }
        cap.available = false;
    }
    return false;
}

bool EncoderManager::SelectAndInitEncoder() {
    if (m_sessionLocked) {
        QualityProfile profile = GetProfileForTier(m_currentTier);
        if (!m_encoder) m_encoder = std::make_unique<FFmpegHardwareEncoder>();
        if (m_encoder->Initialize(profile.width, profile.height, m_fps, profile.bitrateKbps, m_d3d11Device, profile.preset, m_lockedCodecName)) {
            m_encoder->ForceKeyframe(); return true;
        }
        return EmergencyEncoderFallback();
    }
    if (m_backendIndex >= m_capabilities.size()) return false;
    auto& cap = m_capabilities[m_backendIndex];
    m_currentBackend = cap.backend;
    QualityProfile profile = GetProfileForTier(m_currentTier);
    m_encoder = std::make_unique<FFmpegHardwareEncoder>();
    if (m_encoder->Initialize(profile.width, profile.height, m_fps, profile.bitrateKbps, m_d3d11Device, profile.preset, cap.codecName)) {
        m_encoder->ForceKeyframe(); return true;
    }
    cap.available = false; return Fallback();
}

QualityProfile EncoderManager::GetProfileForTier(QualityTier tier) const {
    switch (tier) {
        case QualityTier::TierA_HighPerformance: return { m_baseWidth, m_baseHeight, 15000, 2, "High Performance (Tier A)" };
        case QualityTier::TierB_Balanced: return { m_baseWidth, m_baseHeight, 8000, 1, "Balanced (Tier B)" };
        case QualityTier::TierC_LowEnd: return { 1280, 720, 4000, 0, "Low-End (Tier C)" };
        case QualityTier::TierD_Recovery: return { 854, 480, 1500, 0, "Recovery Mode (Tier D)" };
    }
    return { 1280, 720, 5000, 1, "Default" };
}

bool EncoderManager::Fallback() { return EmergencyEncoderFallback(); }

bool EncoderManager::EmergencyEncoderFallback() {
    SetState(StreamingState::EMERGENCY_FALLBACK);
    m_lockedCodecName = "libx264"; m_currentBackend = EncoderBackend::Software;
    QualityProfile profile = GetProfileForTier(m_currentTier);
    if (!m_encoder) m_encoder = std::make_unique<FFmpegHardwareEncoder>();
    if (m_encoder->Initialize(profile.width, profile.height, m_fps, profile.bitrateKbps, m_d3d11Device, profile.preset, m_lockedCodecName)) {
        m_encoder->ForceKeyframe(); return true;
    }
    return false;
}

void EncoderManager::RequestKeyframe() { if (m_encoder) m_encoder->ForceKeyframe(); }

bool EncoderManager::EncodeFrame(void* texturePtr, std::vector<EncodedPacket>& outPackets, PacketPool& pool) {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    if (m_state == StreamingState::DEGRADED && (rand() % 100 < 20)) return true;
    if (!m_encoder || !m_encoder->IsInitialized()) {
        if (m_sessionLocked) { if (!SelectAndInitEncoder()) return false; } else return false;
    }
    if (!m_encoder->EncodeFrame(texturePtr, outPackets, pool)) return Fallback();
    return true;
}

void EncoderManager::UpdatePerformanceMetrics(float frameDropRate, float avgEncodeTimeMs, float clientDecodeTimeMs, int targetBitrateKbps, AI::LatencyPrediction* aiPrediction) {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    if (targetBitrateKbps > 0 && m_encoder) m_encoder->SetBitrate(targetBitrateKbps);
    if (frameDropRate >= 0) m_lastFrameDropRate = frameDropRate;
    if (avgEncodeTimeMs >= 0) m_lastAvgEncodeTimeMs = avgEncodeTimeMs;
    AI::StreamState aiState; aiState.currentBitrateKbps = GetCurrentBitrate(); aiState.currentWidth = m_baseWidth; aiState.currentHeight = m_baseHeight;
    aiState.currentFPS = m_fps; aiState.packetLoss = frameDropRate; aiState.rttMs = 0;
    AI::LatencyPrediction activePred = aiPrediction ? *aiPrediction : AI::LatencyPrediction{0, 0, frameDropRate, 1.0f};
    auto decision = m_streamEngine->Analyze(aiState, activePred);
    if (decision.action == AI::AdaptationAction::EmergencyThrottle || decision.action == AI::AdaptationAction::DecreaseBitrate) {
        if (m_encoder) m_encoder->SetBitrate(decision.targetBitrateKbps);
    }
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - m_lastAdjustmentTime < 5000) return;
    bool needsDowngrade = false; float frameBudgetMs = 1000.0f / m_fps;
    if (m_lastFrameDropRate > 0.10f || m_lastAvgEncodeTimeMs > frameBudgetMs * 0.8f) needsDowngrade = true;
    if (clientDecodeTimeMs > 0 && clientDecodeTimeMs > frameBudgetMs * 0.8f) needsDowngrade = true;
    if (needsDowngrade) {
        if (m_currentTier != QualityTier::TierD_Recovery) SetState(StreamingState::DEGRADED);
        AdjustTier(false); m_lastAdjustmentTime = now;
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
        QualityProfile oldProfile = GetProfileForTier(oldTier);
        QualityProfile newProfile = GetProfileForTier(m_currentTier);
        if (m_encoder && oldProfile.width == newProfile.width && oldProfile.height == newProfile.height) {
            m_encoder->SetBitrate(newProfile.bitrateKbps);
        } else SelectAndInitEncoder();
    }
}

void EncoderManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    SetState(StreamingState::SHUTDOWN);
    if (m_encoder) { m_encoder->Shutdown(); m_encoder.reset(); }
}

} // namespace Host
