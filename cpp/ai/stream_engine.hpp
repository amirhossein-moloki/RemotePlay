#pragma once

#include <cstdint>
#include <string>
#include "latency_predictor.hpp"

namespace AI {

enum class AdaptationAction {
    Maintain,
    IncreaseBitrate,
    DecreaseBitrate,
    IncreaseResolution,
    DecreaseResolution,
    IncreaseFPS,
    DecreaseFPS,
    ForceKeyframe,
    EmergencyThrottle
};

struct StreamState {
    int currentBitrateKbps;
    int currentWidth;
    int currentHeight;
    int currentFPS;
    float packetLoss;
    float rttMs;
    float sceneComplexity; // 0.0 to 1.0 (heuristic based on frame deltas)
};

struct AdaptationDecision {
    AdaptationAction action;
    int targetBitrateKbps;
    int targetWidth;
    int targetHeight;
    int targetFPS;
    std::string reason;
};

class StreamEngine {
public:
    StreamEngine();

    AdaptationDecision Analyze(const StreamState& state, const LatencyPrediction& prediction);

private:
    uint64_t m_lastActionTime = 0;
    const uint64_t ACTION_COOLDOWN_MS = 2000;
};

} // namespace AI
