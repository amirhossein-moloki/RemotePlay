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

    void PushFrame(std::unique_ptr<FrameData> frame);
    std::unique_ptr<FrameData> PopFrame();

private:
    FixedRingBuffer<std::unique_ptr<FrameData>, 32> m_ring;
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
};

} // namespace Client
