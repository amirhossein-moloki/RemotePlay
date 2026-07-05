#pragma once

#include <deque>
#include <cstdint>

namespace AI {

struct InputSample {
    int32_t x;
    int32_t y;
    uint64_t timestamp;
};

class InputPredictor {
public:
    InputPredictor(size_t historySize = 10);

    void RecordInput(int32_t x, int32_t y, uint64_t timestamp);
    InputSample Predict(uint64_t targetTimestamp);

private:
    size_t m_historySize;
    std::deque<InputSample> m_history;
};

} // namespace AI
