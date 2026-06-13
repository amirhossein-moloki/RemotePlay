#include "receiver.hpp"

namespace Client {

void FrameData::Reset() {
    sequence = 0;
    totalSize = 0;
    fragmentsReceived = 0;
    totalFragments = 0;
    timestamp = 0;
    isComplete = false;
}

Receiver::Receiver(size_t poolSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < poolSize; ++i) {
        auto frame = std::make_unique<FrameData>();
        frame->buffer.resize(1024 * 1024);
        m_framePool.push_back(std::move(frame));
    }
}

void Receiver::ProcessPacket(const Protocol::VideoHeader& header, const uint8_t* payload) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (header.sequence < m_lastCompletedSequence && m_lastCompletedSequence != 0) {
        return;
    }

    auto it = m_pendingFrames.find(header.sequence);
    if (it == m_pendingFrames.end()) {
        if (m_pendingFrames.size() >= 5) {
            auto oldest = m_pendingFrames.begin();
            ReturnToPoolInternal(std::move(oldest->second));
            m_pendingFrames.erase(oldest);
        }

        auto frame = GetFromPoolInternal();
        if (!frame) return;

        frame->Reset();
        frame->sequence = header.sequence;
        frame->totalFragments = header.totalFragments;
        frame->timestamp = header.timestamp;

        it = m_pendingFrames.emplace(header.sequence, std::move(frame)).first;
    }

    auto& frame = it->second;
    size_t offset = header.fragmentIndex * Protocol::MAX_UDP_PAYLOAD;
    if (offset + header.dataSize <= frame->buffer.size()) {
        std::memcpy(frame->buffer.data() + offset, payload, header.dataSize);
        frame->fragmentsReceived++;

        if (header.fragmentIndex == header.totalFragments - 1) {
            frame->totalSize = offset + header.dataSize;
        }
    }

    if (frame->fragmentsReceived == frame->totalFragments) {
        frame->isComplete = true;
        m_lastCompletedSequence = header.sequence;
    }
}

std::unique_ptr<FrameData> Receiver::GetNextFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pendingFrames.empty()) return nullptr;

    auto it = m_pendingFrames.begin();
    if (it->second->isComplete) {
        auto frame = std::move(it->second);
        m_pendingFrames.erase(it);
        return frame;
    }
    return nullptr;
}

void Receiver::ReturnToPool(std::unique_ptr<FrameData> frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ReturnToPoolInternal(std::move(frame));
}

std::unique_ptr<FrameData> Receiver::GetFromPoolInternal() {
    if (m_framePool.empty()) {
        auto frame = std::make_unique<FrameData>();
        frame->buffer.resize(1024 * 1024);
        return frame;
    }
    auto frame = std::move(m_framePool.back());
    m_framePool.pop_back();
    return frame;
}

void Receiver::ReturnToPoolInternal(std::unique_ptr<FrameData> frame) {
    if (frame) {
        m_framePool.push_back(std::move(frame));
    }
}

} // namespace Client
