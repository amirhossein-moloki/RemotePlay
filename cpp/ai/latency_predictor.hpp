#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <cstdint>

namespace AI {

struct NetworkMetrics {
    float rttMs;
    float jitterMs;
    float packetLossRate;
    uint64_t timestamp;
};

struct LatencyPrediction {
    float predictedRttMs;
    float predictedJitterMs;
    float predictedLossRate;
    float confidenceScore; // 0.0 to 1.0
};

class LatencyPredictor {
public:
    LatencyPredictor(size_t historySize = 100);

    void RecordMetrics(const NetworkMetrics& metrics);
    LatencyPrediction Predict(uint32_t lookaheadMs = 100);

private:
    float CalculateEWMA(const std::deque<float>& data, float alpha);
    float CalculateTrend(const std::deque<float>& data);

    size_t m_historySize;
    std::deque<float> m_rttHistory;
    std::deque<float> m_jitterHistory;
    std::deque<float> m_lossHistory;
    std::deque<uint64_t> m_timestampHistory;

    float m_rttEwma = 0.0f;
    float m_jitterEwma = 0.0f;
    float m_lossEwma = 0.0f;

    std::mutex m_mutex;

    const float ALPHA_FAST = 0.3f;
    const float ALPHA_SLOW = 0.1f;
};

} // namespace AI
