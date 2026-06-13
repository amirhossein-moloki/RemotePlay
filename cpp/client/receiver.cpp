#include "receiver.hpp"
#include <iostream>

namespace Client {

void FrameData::Reset() {
    frameId = 0;
    totalSize = 0;
    fragmentsReceived = 0;
    totalFragments = 0;
    timestamp = 0;
    isComplete = false;
    std::fill(fragmentMap.begin(), fragmentMap.end(), false);
    std::fill(fragmentSizes.begin(), fragmentSizes.end(), 0);
}

Receiver::Receiver(size_t poolSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < poolSize; ++i) {
        auto frame = std::make_unique<FrameData>();
        frame->buffer.resize(1024 * 1024);
        frame->fragmentMap.resize(1024);
        frame->fragmentSizes.resize(1024);
        m_framePool.push_back(std::move(frame));
    }
}

void Receiver::ProcessPacket(const Protocol::VideoHeader& header, const uint8_t* payload) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_firstFrameReceived) {
        m_nextFrameIdToRead = header.frameId;
        m_firstFrameReceived = true;
    }

    if (header.frameId < m_nextFrameIdToRead) return;

    auto* framePtr = m_frameRing.get(header.frameId);
    if (!*framePtr || (*framePtr)->frameId != header.frameId) {
        if (*framePtr) ReturnToPoolInternal(std::move(*framePtr));

        auto newFrame = GetFromPoolInternal();
        newFrame->Reset();
        newFrame->frameId = header.frameId;
        newFrame->totalFragments = header.totalFragments;
        newFrame->timestamp = header.timestamp;
        if (newFrame->fragmentMap.size() < header.totalFragments) {
            newFrame->fragmentMap.resize(header.totalFragments, false);
            newFrame->fragmentSizes.resize(header.totalFragments, 0);
        }
        m_frameRing.insert(header.frameId, std::move(newFrame));
    }

    FrameData* frame = m_frameRing.get(header.frameId)->get();
    if (header.fragmentIndex < frame->fragmentMap.size() && !frame->fragmentMap[header.fragmentIndex]) {
        size_t offset = header.fragmentIndex * Protocol::MAX_UDP_PAYLOAD;
        if (offset + header.dataSize <= frame->buffer.size()) {
            std::memcpy(frame->buffer.data() + offset, payload, header.dataSize);
            frame->fragmentMap[header.fragmentIndex] = true;
            frame->fragmentSizes[header.fragmentIndex] = header.dataSize;
            frame->fragmentsReceived++;

            if (header.fragmentIndex == header.totalFragments - 1) {
                frame->totalSize = offset + header.dataSize;
            }
        }

        const int FEC_GROUP_SIZE = 5;
        uint16_t groupStart = (header.fragmentIndex / FEC_GROUP_SIZE) * FEC_GROUP_SIZE;
        TryRecover(header.frameId, groupStart);
    }

    if (frame->fragmentsReceived == frame->totalFragments) {
        frame->isComplete = true;
        m_lastCompletedFrameId = std::max(m_lastCompletedFrameId, header.frameId);
    }
}

void Receiver::ProcessFEC(const Protocol::FECHeader& header, const uint8_t* payload) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (header.frameId < m_nextFrameIdToRead) return;

    // Improved indexing for FEC ring to avoid collisions
    uint32_t fecIdx = (header.frameId * 13) + (header.fragmentStart / 5);
    auto* group = m_fecRing.get(fecIdx);

    if (!group->fecReceived || group->frameId != header.frameId || group->fecHeader.fragmentStart != header.fragmentStart) {
        if (group->fecPacket) m_fecPool.release(std::move(group->fecPacket));

        group->fecHeader = header;
        group->frameId = header.frameId;
        group->fecPacket = m_fecPool.acquire();
        memcpy(group->fecPacket->data.data(), payload, header.dataSize);
        group->fecPacket->size = header.dataSize;
        group->fecReceived = true;

        TryRecover(header.frameId, header.fragmentStart);
    }
}

void Receiver::TryRecover(uint32_t frameId, uint16_t groupStart) {
    auto* framePtr = m_frameRing.get(frameId);
    if (!*framePtr || (*framePtr)->frameId != frameId) return;
    FrameData* frame = (*framePtr).get();
    if (frame->isComplete) return;

    uint32_t fecIdx = (frameId * 13) + (groupStart / 5);
    auto* group = m_fecRing.get(fecIdx);
    if (!group->fecReceived || group->frameId != frameId || group->fecHeader.fragmentStart != groupStart) return;

    uint16_t missingIndex = 0;
    uint16_t missingCount = 0;
    for (uint16_t i = 0; i < group->fecHeader.fragmentCount; ++i) {
        uint16_t idx = groupStart + i;
        if (idx >= frame->totalFragments) break;
        if (!frame->fragmentMap[idx]) {
            missingCount++;
            missingIndex = idx;
        }
    }

    if (missingCount == 1) {
        size_t offset = missingIndex * Protocol::MAX_UDP_PAYLOAD;
        uint8_t* recoveredPayload = frame->buffer.data() + offset;

        // Zero-fill recovered buffer before XOR to avoid stale data
        memset(recoveredPayload, 0, group->fecHeader.dataSize);

        // Logical XOR with zero-padding for fragments smaller than fecHeader.dataSize
        memcpy(recoveredPayload, group->fecPacket->data.data(), group->fecHeader.dataSize);

        for (uint16_t i = 0; i < group->fecHeader.fragmentCount; ++i) {
            uint16_t idx = groupStart + i;
            if (idx == missingIndex) continue;

            uint8_t* otherPayload = frame->buffer.data() + (idx * Protocol::MAX_UDP_PAYLOAD);
            uint16_t otherSize = frame->fragmentSizes[idx];

            for (size_t k = 0; k < group->fecHeader.dataSize; ++k) {
                uint8_t byte = (k < otherSize) ? otherPayload[k] : 0;
                recoveredPayload[k] ^= byte;
            }
        }

        frame->fragmentMap[missingIndex] = true;
        frame->fragmentSizes[missingIndex] = group->fecHeader.dataSize; // Correct if all frags same size, but for last frag might be slightly off.
        // Actually FEC dataSize is max(frags in group). If missingIndex is last, we assume it's dataSize.

        frame->fragmentsReceived++;
        if (missingIndex == frame->totalFragments - 1) {
            frame->totalSize = offset + group->fecHeader.dataSize;
        }

        if (frame->fragmentsReceived == frame->totalFragments) {
            frame->isComplete = true;
            m_lastCompletedFrameId = std::max(m_lastCompletedFrameId, frameId);
        }
    }
}

std::unique_ptr<FrameData> Receiver::GetNextFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t newestComplete = 0;
    bool found = false;

    for (uint32_t i = 0; i < 32; ++i) {
        uint32_t fid = m_nextFrameIdToRead + i;
        auto* framePtr = m_frameRing.get(fid);
        if (*framePtr && (*framePtr)->frameId == fid && (*framePtr)->isComplete) {
            newestComplete = fid;
            found = true;
        }
    }

    if (found) {
        for (uint32_t fid = m_nextFrameIdToRead; fid < newestComplete; ++fid) {
            auto* framePtr = m_frameRing.get(fid);
            if (*framePtr && (*framePtr)->frameId == fid) {
                ReturnToPoolInternal(std::move(*framePtr));
            }
        }

        m_nextFrameIdToRead = newestComplete;
        auto* framePtr = m_frameRing.get(newestComplete);
        auto frame = std::move(*framePtr);
        m_nextFrameIdToRead++;
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
        frame->fragmentMap.resize(1024);
        frame->fragmentSizes.resize(1024);
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
