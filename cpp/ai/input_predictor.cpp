#include "input_predictor.hpp"
#include <numeric>

namespace AI {

InputPredictor::InputPredictor(size_t historySize) : m_historySize(historySize) {}

void InputPredictor::RecordInput(int32_t x, int32_t y, uint64_t timestamp) {
    m_history.push_back({x, y, timestamp});
    if (m_history.size() > m_historySize) {
        m_history.pop_front();
    }

    if (m_lastSmoothX == 0 && m_lastSmoothY == 0) {
        m_lastSmoothX = (float)x;
        m_lastSmoothY = (float)y;
    }
}

InputSample InputPredictor::Predict(uint64_t targetTimestamp) {
    if (m_history.size() < 2) {
        if (m_history.empty()) return {0, 0, targetTimestamp};
        return {m_history.back().x, m_history.back().y, targetTimestamp};
    }

    // Linear extrapolation based on last two samples
    const auto& s1 = m_history[m_history.size() - 2];
    const auto& s2 = m_history.back();

    if (s1.timestamp == s2.timestamp) return {s2.x, s2.y, targetTimestamp};

    double dt = (double)(targetTimestamp - s2.timestamp);
    double vdt = (double)(s2.timestamp - s1.timestamp);

    double vx = (double)(s2.x - s1.x) / vdt;
    double vy = (double)(s2.y - s1.y) / vdt;

    InputSample prediction;
    prediction.x = s2.x + (int32_t)(vx * dt);
    prediction.y = s2.y + (int32_t)(vy * dt);
    prediction.timestamp = targetTimestamp;

    return prediction;
}

InputSample InputPredictor::Smooth(const InputSample& raw) {
    if (m_lastSmoothX == 0 && m_lastSmoothY == 0) {
        m_lastSmoothX = (float)raw.x;
        m_lastSmoothY = (float)raw.y;
        return raw;
    }

    m_lastSmoothX = m_lastSmoothX * SMOOTHING_FACTOR + (float)raw.x * (1.0f - SMOOTHING_FACTOR);
    m_lastSmoothY = m_lastSmoothY * SMOOTHING_FACTOR + (float)raw.y * (1.0f - SMOOTHING_FACTOR);

    InputSample smoothed = raw;
    smoothed.x = (int32_t)m_lastSmoothX;
    smoothed.y = (int32_t)m_lastSmoothY;
    return smoothed;
}

} // namespace AI
