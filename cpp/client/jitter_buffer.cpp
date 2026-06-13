#include "jitter_buffer.hpp"
#include <algorithm>

namespace Client {

JitterBuffer::JitterBuffer(size_t maxFrames) : m_maxFrames(maxFrames) {}

void JitterBuffer::PushFrame(Receiver::FramePtr frame) {
    if (!frame) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        m_nextPopFrameId = frame->frameId;
        m_initialized = true;
    }

    if (frame->frameId < m_nextPopFrameId) return;

    // Adaptive jitter estimation
    auto now = std::chrono::steady_clock::now();
    if (m_hasLastPushTime) {
        auto delta = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastPushTime).count() / 1000.0;
        // Simple EWMA for jitter
        double currentJitter = std::abs(delta - 16.67); // Assuming 60fps
        m_avgJitterMs = m_avgJitterMs * 0.9 + currentJitter * 0.1;
    }
    m_lastPushTime = now;
    m_hasLastPushTime = true;

    auto* existing = m_ring.get(frame->frameId);
    if (*existing && (*existing)->frameId == frame->frameId) return; // Duplicate

    uint32_t fid = frame->frameId;
    m_ring.insert(fid, std::move(frame));
    EntryInfo info;
    info.frameId = fid;
    info.pushTime = std::chrono::steady_clock::now();
    info.occupied = true;
    m_info.insert(info.frameId, info);
    m_count++;

    while (m_count > m_maxFrames) {
        // Drop oldest
        uint32_t oldestId = 0xffffffff;
        bool found = false;
        for (uint32_t i = 0; i < 32; ++i) {
            auto* inf = m_info.get(m_nextPopFrameId + i);
            if (inf->occupied && inf->frameId < oldestId) {
                oldestId = inf->frameId;
                found = true;
            }
        }
        if (found) {
            auto* entry = m_ring.get(oldestId);
            *entry = nullptr;
            m_info.get(oldestId)->occupied = false;
            m_count--;
            if (oldestId == m_nextPopFrameId) m_nextPopFrameId++;
        } else break;
    }
}

Receiver::FramePtr JitterBuffer::PopFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find newest available frame to avoid HoL blocking
    uint32_t bestFid = 0;
    bool found = false;

    // Target delay is based on measured jitter.
    // On stable LAN, avgJitterMs will be < 1ms, allowing near-immediate pop.
    double targetDelayMs = std::clamp(m_avgJitterMs * 1.1, 1.0, 20.0);

    for (uint32_t i = 0; i < 32; ++i) {
        uint32_t fid = m_nextPopFrameId + i;
        auto* info = m_info.get(fid);
        if (info->occupied && info->frameId == fid) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - info->pushTime).count() / 1000.0;

            // If it's been in the buffer long enough, or buffer is full, or it's a newer frame and we want to catch up
            if (elapsed >= targetDelayMs || m_count >= m_maxFrames) {
                bestFid = fid;
                found = true;
                // If we are over-buffered, prioritize the newest frame to reduce latency
                if (m_count >= m_maxFrames) continue;
                else break;
            }
        }
    }

    if (found) {
        // Skip anything older than bestFid
        for (uint32_t fid = m_nextPopFrameId; fid < bestFid; ++fid) {
            auto* entry = m_ring.get(fid);
            if (*entry) {
                *entry = nullptr;
                m_info.get(fid)->occupied = false;
                m_count--;
            }
        }

        m_nextPopFrameId = bestFid;
        auto* entry = m_ring.get(bestFid);
        auto frame = std::move(*entry);
        m_info.get(bestFid)->occupied = false;
        m_count--;
        m_nextPopFrameId++;
        return frame;
    }

    return Receiver::FramePtr(nullptr, Receiver::FrameDeleter{nullptr});
}

} // namespace Client
