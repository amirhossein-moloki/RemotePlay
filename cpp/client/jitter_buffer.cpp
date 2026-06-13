#include "jitter_buffer.hpp"
#include <algorithm>

namespace Client {

JitterBuffer::JitterBuffer(size_t maxFrames) : m_maxFrames(maxFrames) {}

void JitterBuffer::PushFrame(std::unique_ptr<FrameData> frame) {
    if (!frame) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        m_nextPopFrameId = frame->frameId;
        m_initialized = true;
    }

    if (frame->frameId < m_nextPopFrameId) return;

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

std::unique_ptr<FrameData> JitterBuffer::PopFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find newest available frame to avoid HoL blocking
    uint32_t bestFid = 0;
    bool found = false;

    for (uint32_t i = 0; i < 32; ++i) {
        uint32_t fid = m_nextPopFrameId + i;
        auto* info = m_info.get(fid);
        if (info->occupied && info->frameId == fid) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - info->pushTime).count();

            // If it's been in the buffer long enough, or buffer is full, or it's a newer frame and we want to catch up
            if (elapsed >= 10 || m_count >= m_maxFrames) {
                bestFid = fid;
                found = true;
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

    return nullptr;
}

} // namespace Client
