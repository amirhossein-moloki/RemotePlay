#pragma once

#include <cstdint>
#include <chrono>
#include <atomic>
#include <vector>
#include <algorithm>

namespace Common {

class ClockSync {
public:
    void ProcessResponse(uint64_t t0, uint64_t t1, uint64_t t2, uint64_t t3) {
        // t0: Client send
        // t1: Host receive
        // t2: Host send
        // t3: Client receive

        int64_t rtt = (int64_t)(t3 - t0) - (int64_t)(t2 - t1);
        int64_t offset = ((int64_t)(t1 - t0) + (int64_t)(t2 - t3)) / 2;

        m_rtt = (uint64_t)std::max((int64_t)0, rtt);
        m_clockOffset = offset;
        m_synchronized = true;
    }

    uint64_t GetHostTime(uint64_t clientTime) const {
        return (uint64_t)((int64_t)clientTime + m_clockOffset);
    }

    uint64_t GetClientTime(uint64_t hostTime) const {
        return (uint64_t)((int64_t)hostTime - m_clockOffset);
    }

    bool IsSynchronized() const { return m_synchronized; }
    uint64_t GetRTT() const { return m_rtt; }

private:
    std::atomic<int64_t> m_clockOffset{0};
    std::atomic<uint64_t> m_rtt{0};
    std::atomic<bool> m_synchronized{false};
};

} // namespace Common
