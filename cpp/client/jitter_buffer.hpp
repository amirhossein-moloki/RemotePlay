#pragma once

#include <memory>
#include <mutex>
#include <chrono>
#include "receiver.hpp"
#include "../common/fixed_ring_buffer.hpp"

namespace Client {

class JitterBuffer {
public:
    JitterBuffer(size_t maxFrames = 3);

    void PushFrame(Receiver::FramePtr frame);
    Receiver::FramePtr PopFrame();
    Receiver::FramePtr PopFrame(uint64_t currentSyncTimeUs);
    void Reset(uint32_t nextExpectedId);

private:
    FixedRingBuffer<Receiver::FramePtr, 32> m_ring;
    struct EntryInfo {
        uint32_t frameId = 0;
        std::chrono::steady_clock::time_point pushTime;
        bool occupied = false;
    };
    FixedRingBuffer<EntryInfo, 32> m_info;

    uint32_t m_nextPopFrameId = 0;
    bool m_initialized = false;
    size_t m_count = 0;
    size_t m_maxFrames;
    std::mutex m_mutex;

    double m_avgJitterMs = 5.0;
    std::chrono::steady_clock::time_point m_lastPushTime;
    bool m_hasLastPushTime = false;
};

} // namespace Client
