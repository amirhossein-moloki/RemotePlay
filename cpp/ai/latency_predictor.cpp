#include "latency_predictor.hpp"
#include <numeric>
#include <algorithm>
#include <cmath>

namespace AI {

LatencyPredictor::LatencyPredictor(size_t historySize) : m_historySize(historySize) {}

void LatencyPredictor::RecordMetrics(const NetworkMetrics& metrics) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_rttHistory.push_back(metrics.rttMs);
    m_jitterHistory.push_back(metrics.jitterMs);
    m_lossHistory.push_back(metrics.packetLossRate);
    m_timestampHistory.push_back(metrics.timestamp);

    if (m_rttHistory.size() > m_historySize) {
        m_rttHistory.pop_front();
        m_jitterHistory.pop_front();
        m_lossHistory.pop_front();
        m_timestampHistory.pop_front();
    }

    // Update EWMAs
    if (m_rttHistory.size() == 1) {
        m_rttEwma = metrics.rttMs;
        m_jitterEwma = metrics.jitterMs;
        m_lossEwma = metrics.packetLossRate;
    } else {
        m_rttEwma = ALPHA_FAST * metrics.rttMs + (1.0f - ALPHA_FAST) * m_rttEwma;
        m_jitterEwma = ALPHA_FAST * metrics.jitterMs + (1.0f - ALPHA_FAST) * m_jitterEwma;
        m_lossEwma = ALPHA_SLOW * metrics.packetLossRate + (1.0f - ALPHA_SLOW) * m_lossEwma;
    }
}

LatencyPrediction LatencyPredictor::Predict(uint32_t lookaheadMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    LatencyPrediction prediction;
    if (m_rttHistory.empty()) {
        return {0, 0, 0, 0};
    }

    float rttTrend = CalculateTrend(m_rttHistory);
    float jitterTrend = CalculateTrend(m_jitterHistory);
    float lossTrend = CalculateTrend(m_lossHistory);

    // Simple linear extrapolation for short-term forecasting
    prediction.predictedRttMs = std::max(0.0f, m_rttEwma + rttTrend * (lookaheadMs / 100.0f));
    prediction.predictedJitterMs = std::max(0.0f, m_jitterEwma + jitterTrend * (lookaheadMs / 100.0f));
    prediction.predictedLossRate = std::clamp(m_lossEwma + lossTrend * (lookaheadMs / 100.0f), 0.0f, 1.0f);

    // Confidence decreases with lookahead and historical variance
    prediction.confidenceScore = std::max(0.0f, 1.0f - (lookaheadMs / 1000.0f));

    return prediction;
}

float LatencyPredictor::CalculateTrend(const std::deque<float>& data) {
    if (data.size() < 5) return 0.0f;

    // Calculate slope of last few points
    size_t n = 5;
    float sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    for (size_t i = data.size() - n; i < data.size(); ++i) {
        float x = (float)(i - (data.size() - n));
        float y = data[i];
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumXX += x * x;
    }

    float slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
    return slope;
}

} // namespace AI
