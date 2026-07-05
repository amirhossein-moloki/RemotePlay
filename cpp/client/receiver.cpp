#include "receiver.hpp"
#include <iostream>
#include "common/profiler.hpp"
#include "common/logger.hpp"

namespace Client {

void FrameData::Reset() {
    frameId = 0;
    totalSize = 0;
    fragmentsReceived = 0;
    totalFragments = 0;
    captureTimestamp = 0;
    encodeStartTimestamp = 0;
    encodeEndTimestamp = 0;
    receiveTimestamp = 0;
    isComplete = false;
    isKeyframe = false;
    isHEVC = false;
    std::fill(fragmentMap.begin(), fragmentMap.end(), false);
    std::fill(fragmentSizes.begin(), fragmentSizes.end(), 0);
}

Receiver::Receiver(size_t poolSize) {
    // No lock needed in constructor
    for (size_t i = 0; i < poolSize; ++i) {
        auto frame = std::make_unique<FrameData>();
        frame->buffer.resize(1024 * 1024);
        frame->fragmentMap.resize(1024);
        frame->fragmentSizes.resize(1024);
        m_framePool.push_back(frame.get());
        m_frameStorage.push_back(std::move(frame));
    }
    for (size_t i = 0; i < 64; ++i) {
        *m_frameRing.get((uint32_t)i) = nullptr;
    }
    m_sequenceWindow.resize(WINDOW_SIZE / 64, 0);
}

std::vector<Receiver::NACK> Receiver::GetPendingNACKs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<NACK> nacks;

    // Only request retransmission for frames that are relatively recent
    // and aren't too far ahead of our current read pointer to avoid huge bursts.
    for (uint32_t i = 0; i < 32; ++i) {
        uint32_t fid = m_nextFrameIdToRead + i;
        FrameData** framePtr = m_frameRing.get(fid);
        if (*framePtr && (*framePtr)->frameId == fid && !(*framePtr)->isComplete) {
            for (uint16_t j = 0; j < (*framePtr)->totalFragments; ++j) {
                if (!(*framePtr)->fragmentMap[j]) {
                    // Limit NACKs per call to avoid flooding
                    if (nacks.size() >= 10) return nacks;
                    nacks.push_back({ fid, j });
                }
            }
        }
    }
    return nacks;
}

void Receiver::ProcessPacket(const Protocol::VideoHeader& header, const uint8_t* payload) {
    ScopeTimer timer("Receiver_ProcessPacket");
    LOG_INFO("StreamTrace", "REASSEMBLY_FRAGMENT_IN frameId=" + std::to_string(header.frameId) +
             " fragment=" + std::to_string(header.fragmentIndex) + "/" + std::to_string(header.totalFragments) +
             " bytes=" + std::to_string(header.dataSize));
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_firstFrameReceived) {
        // Only start reassembly upon receiving a fragment from a Keyframe (IDR).
        // This ensures the decoder starts with valid SPS/PPS headers.
        if (!(header.flags & 0x01)) {
            LOG_INFO("Receiver", "Ignoring initial P-frame fragment to wait for Keyframe.");
            return;
        }
        m_nextFrameIdToRead = header.frameId;
        m_firstFrameReceived = true;
    }

    if (header.frameId < m_nextFrameIdToRead) return;

    FrameData** framePtr = m_frameRing.get(header.frameId);
    if (!*framePtr || (*framePtr)->frameId != header.frameId) {
        if (*framePtr) ReturnToPoolInternalRaw(*framePtr);

        FrameData* newFrame = GetFromPoolInternalRaw();
        if (!newFrame) return;

        newFrame->Reset();
        newFrame->frameId = header.frameId;
        newFrame->totalFragments = header.totalFragments;
        newFrame->captureTimestamp = header.captureTimestamp;
        newFrame->encodeStartTimestamp = header.encodeStartTimestamp;
        newFrame->encodeEndTimestamp = header.encodeEndTimestamp;
        newFrame->isKeyframe = (header.flags & 0x01) != 0;
        newFrame->isHEVC = (header.flags & 0x02) != 0;
        newFrame->receiveTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        if (newFrame->fragmentMap.size() < header.totalFragments) {
            newFrame->fragmentMap.resize(header.totalFragments, false);
            newFrame->fragmentSizes.resize(header.totalFragments, 0);
        }
        m_frameRing.insert(header.frameId, std::move(newFrame));
    }

    FrameData* frame = *m_frameRing.get(header.frameId);
    if (header.fragmentIndex < frame->fragmentMap.size() && !frame->fragmentMap[header.fragmentIndex]) {
        size_t offset = header.fragmentIndex * Protocol::MAX_UDP_PAYLOAD;
        if (offset + header.dataSize <= frame->buffer.size()) {
            std::memcpy(frame->buffer.data() + offset, payload, header.dataSize);
            frame->fragmentMap[header.fragmentIndex] = true;
            frame->fragmentSizes[header.fragmentIndex] = header.dataSize;
            frame->fragmentsReceived++;
            LOG_INFO("StreamTrace", "REASSEMBLY_FRAGMENT_STORED frameId=" + std::to_string(header.frameId) +
                     " received=" + std::to_string(frame->fragmentsReceived) +
                     "/" + std::to_string(frame->totalFragments));

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
        LOG_INFO("StreamTrace", "REASSEMBLY_FRAME_COMPLETE frameId=" + std::to_string(header.frameId) +
                 " totalSize=" + std::to_string(frame->totalSize) +
                 " fragments=" + std::to_string(frame->totalFragments));
    }
}

void Receiver::ProcessFEC(const Protocol::FECHeader& header, const uint8_t* payload) {
    ScopeTimer timer("Receiver_ProcessFEC");
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
    ScopeTimer timer("Receiver_TryRecover");
    FrameData** framePtr = m_frameRing.get(frameId);
    if (!*framePtr || (*framePtr)->frameId != frameId) return;
    FrameData* frame = *framePtr;
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
        if (offset + group->fecHeader.dataSize > frame->buffer.size()) return;

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
            size_t xorSize = std::min((size_t)group->fecHeader.dataSize, (size_t)otherSize);

            for (size_t k = 0; k < xorSize; ++k) {
                recoveredPayload[k] ^= otherPayload[k];
            }
            // Bytes beyond otherSize in recoveredPayload are already XORed with 0 (no-op)
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

Receiver::FramePtr Receiver::GetNextFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Enforce strict ordering: only pop the next expected frame if it is complete.
    // Skipping frames here causes H.264 decoding artifacts (flickering).
    auto* framePtr = m_frameRing.get(m_nextFrameIdToRead);
    if (*framePtr && (*framePtr)->frameId == m_nextFrameIdToRead && (*framePtr)->isComplete) {
        FrameData* frameRaw = *framePtr;
        *framePtr = nullptr;
        m_nextFrameIdToRead++;

        LOG_INFO("StreamTrace", "REASSEMBLY_FRAME_POP frameId=" + std::to_string(frameRaw->frameId) +
                 " totalSize=" + std::to_string(frameRaw->totalSize));
        return FramePtr(frameRaw, FrameDeleter{this});
    }

    return FramePtr(nullptr, FrameDeleter{this});
}

uint32_t Receiver::FindLatestAvailableKeyframe() {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t latestKeyframeId = 0;
    bool found = false;

    // Scan the ring buffer for the newest complete keyframe ahead of our current read pointer
    for (uint32_t i = 0; i < 64; ++i) {
        uint32_t fid = m_nextFrameIdToRead + i;
        FrameData** framePtr = m_frameRing.get(fid);
        if (*framePtr && (*framePtr)->frameId == fid && (*framePtr)->isComplete && (*framePtr)->isKeyframe) {
            latestKeyframeId = fid;
            found = true;
        }
    }

    return found ? latestKeyframeId : 0;
}

void Receiver::ForceAdvanceTo(uint32_t frameId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (frameId <= m_nextFrameIdToRead) return;

    for (uint32_t fid = m_nextFrameIdToRead; fid < frameId; ++fid) {
        FrameData** framePtr = m_frameRing.get(fid);
        if (*framePtr && (*framePtr)->frameId == fid) {
            ReturnToPoolInternalRaw(*framePtr);
            *framePtr = nullptr;
        }
    }
    m_nextFrameIdToRead = frameId;
}

void Receiver::FrameDeleter::operator()(FrameData* frame) const {
    if (frame && receiver) {
        receiver->ReturnToPoolRaw(frame);
    }
}

void Receiver::ReturnToPool(FramePtr frame) {
    // Just let it go out of scope or reset, deleter handles it
    frame.reset();
}

void Receiver::ReturnToPoolRaw(FrameData* frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ReturnToPoolInternalRaw(frame);
}

FrameData* Receiver::GetFromPoolInternalRaw() {
    if (m_framePool.empty()) {
        // Emergency allocation if pool is too small, though we should avoid this in hot path
        auto frame = std::make_unique<FrameData>();
        frame->buffer.resize(1024 * 1024);
        frame->fragmentMap.resize(1024);
        frame->fragmentSizes.resize(1024);
        FrameData* ptr = frame.get();
        m_frameStorage.push_back(std::move(frame));
        return ptr;
    }
    FrameData* frame = m_framePool.back();
    m_framePool.pop_back();
    return frame;
}

void Receiver::ReturnToPoolInternalRaw(FrameData* frame) {
    if (frame) {
        // Basic duplicate check to prevent double-free to pool
        for (auto it = m_framePool.begin(); it != m_framePool.end(); ++it) {
            if (*it == frame) return;
        }
        m_framePool.push_back(frame);
    }
}

bool Receiver::ValidateSequence(uint64_t seq) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (seq > m_maxSequenceReceived) {
        // High-performance window advancement
        uint64_t diff = seq - m_maxSequenceReceived;
        if (diff >= WINDOW_SIZE) {
            std::fill(m_sequenceWindow.begin(), m_sequenceWindow.end(), 0);
        } else {
            // Bit-level shift for replay window
            for (uint64_t i = 0; i < diff; ++i) {
                uint64_t targetSeq = m_maxSequenceReceived + i + 1;
                size_t wordIdx = (targetSeq / 64) % (WINDOW_SIZE / 64);
                m_sequenceWindow[wordIdx] &= ~(1ULL << (targetSeq % 64));
            }
        }
        m_maxSequenceReceived = seq;
    } else {
        if (m_maxSequenceReceived - seq >= WINDOW_SIZE) return false;
    }

    size_t wordIdx = (seq / 64) % (WINDOW_SIZE / 64);
    uint64_t bit = 1ULL << (seq % 64);

    if (m_sequenceWindow[wordIdx] & bit) return false; // Replay

    m_sequenceWindow[wordIdx] |= bit;
    return true;
}

} // namespace Client
