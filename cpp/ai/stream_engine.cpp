#include "stream_engine.hpp"
#include <chrono>
#include <algorithm>

namespace AI {

StreamEngine::StreamEngine() {
    m_lastActionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

AdaptationDecision StreamEngine::Analyze(const StreamState& state, const LatencyPrediction& prediction) {
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    AdaptationDecision decision;
    decision.action = AdaptationAction::Maintain;
    decision.targetBitrateKbps = state.currentBitrateKbps;
    decision.targetWidth = state.currentWidth;
    decision.targetHeight = state.currentHeight;
    decision.targetFPS = state.currentFPS;

    // Emergency Throttle on high loss or high predicted RTT
    if (prediction.predictedLossRate > 0.15f || prediction.predictedRttMs > 250.0f) {
        decision.action = AdaptationAction::EmergencyThrottle;
        decision.targetBitrateKbps = std::max(500, (int)(state.currentBitrateKbps * 0.5f));
        decision.targetWidth = 854;
        decision.targetHeight = 480;
        decision.targetFPS = 30;
        decision.reason = "Critical network congestion predicted";
        m_lastActionTime = now;
        return decision;
    }

    if (now - m_lastActionTime < ACTION_COOLDOWN_MS) {
        return decision;
    }

    // Motion-Aware Bitrate Adjustment
    float complexityMultiplier = 1.0f + (state.sceneComplexity * 0.5f); // Increase bitrate up to 50% for high motion
    int idealBitrate = (int)(decision.targetBitrateKbps * complexityMultiplier);

    // Proactive Congestion Avoidance
    if (prediction.predictedLossRate > 0.02f || prediction.predictedRttMs > state.rttMs * 1.2f) {
        decision.action = AdaptationAction::DecreaseBitrate;
        decision.targetBitrateKbps = (int)(state.currentBitrateKbps * 0.80f);
        decision.reason = "Predicted latency spike or packet loss";
        m_lastActionTime = now;
    }
    // Recovery / Quality Improvement
    else if (prediction.predictedLossRate < 0.005f && prediction.predictedRttMs < 50.0f && state.packetLoss < 0.001f) {
        if (state.currentBitrateKbps < 30000) {
            decision.action = AdaptationAction::IncreaseBitrate;
            decision.targetBitrateKbps = std::min(30000, idealBitrate);
            decision.reason = "Stable network, motion-aware quality increase";
            m_lastActionTime = now;
        }
    }

    // Resolution scaling logic based on bitrate thresholds
    if (decision.targetBitrateKbps < 2500 && state.currentWidth > 854) {
        decision.action = AdaptationAction::DecreaseResolution;
        decision.targetWidth = 854;
        decision.targetHeight = 480;
        decision.targetFPS = 30;
    } else if (decision.targetBitrateKbps > 12000 && state.currentWidth < 3840 && state.currentWidth >= 1920) {
        decision.action = AdaptationAction::IncreaseResolution;
        decision.targetWidth = 3840;
        decision.targetHeight = 2160;
    } else if (decision.targetBitrateKbps > 6000 && state.currentWidth < 1920) {
        decision.action = AdaptationAction::IncreaseResolution;
        decision.targetWidth = 1920;
        decision.targetHeight = 1080;
        decision.targetFPS = 60;
    }

    return decision;
}

} // namespace AI
