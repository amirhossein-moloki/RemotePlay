#include "jitter_buffer.hpp"
#include <algorithm>
#include "../common/logger.hpp"

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

    // Only drop if we are nearing the absolute capacity of the ring buffer.
    // m_maxFrames (e.g. 10) is the target for PopFrame to maintain latency,
    // but we allow more in the buffer to handle network bursts without breaking the stream.
    // We use a safety margin of 4 below the full capacity to avoid collisions.
    while (m_count > (m_ring.capacity() - 4)) {
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
            LOG_WARN("JitterBuffer", "Buffer overflow (count=" + std::to_string(m_count) + "), dropping oldest frame " + std::to_string(oldestId));
            auto* entry = m_ring.get(oldestId);
            *entry = nullptr;
            m_info.get(oldestId)->occupied = false;
            m_count--;
            if (oldestId == m_nextPopFrameId) m_nextPopFrameId++;
        } else break;
    }
}

Receiver::FramePtr JitterBuffer::PopFrame() {
    return PopFrame(0);
}

Receiver::FramePtr JitterBuffer::PopFrame(uint64_t currentSyncTimeUs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t fid = m_nextPopFrameId;
    auto* info = m_info.get(fid);

    if (info->occupied && info->frameId == fid) {
        auto* entry = m_ring.get(fid);

        // If we have a sync time, use it for A/V alignment
        if (currentSyncTimeUs > 0) {
            if ((*entry)->captureTimestamp > currentSyncTimeUs + 5000) {
                // Too early, wait
                return Receiver::FramePtr(nullptr, Receiver::FrameDeleter{nullptr});
            }
        } else {
            // Original jitter-based logic
            double targetDelayMs = std::clamp(m_avgJitterMs * 1.1, 1.0, 20.0);
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - info->pushTime).count() / 1000.0;

            if (elapsed < targetDelayMs && m_count < m_maxFrames) {
                return Receiver::FramePtr(nullptr, Receiver::FrameDeleter{nullptr});
            }
        }

        auto frame = std::move(*entry);
        info->occupied = false;
        m_count--;
        m_nextPopFrameId++;
        return frame;
    }

    return Receiver::FramePtr(nullptr, Receiver::FrameDeleter{nullptr});
}

void JitterBuffer::Reset(uint32_t nextExpectedId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint32_t i = 0; i < 32; ++i) {
        auto* entry = m_ring.get(m_nextPopFrameId + i);
        if (*entry) {
            *entry = nullptr;
        }
        auto* info = m_info.get(m_nextPopFrameId + i);
        info->occupied = false;
    }
    m_nextPopFrameId = nextExpectedId;
    m_count = 0;
    m_initialized = true;
}

} // namespace Client
