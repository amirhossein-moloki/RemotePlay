#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <cstring>
#include <unordered_map>
#include "../common/protocol.hpp"
#include "../common/fixed_ring_buffer.hpp"
#include "../common/packet_pool.hpp"

namespace Client {

struct FrameData {
    uint32_t frameId;
    std::vector<uint8_t> buffer;
    size_t totalSize;
    uint16_t fragmentsReceived;
    uint16_t totalFragments;
    uint64_t captureTimestamp;
    uint64_t encodeStartTimestamp;
    uint64_t encodeEndTimestamp;
    uint64_t receiveTimestamp;
    bool isComplete;
    bool isKeyframe;
    bool isHEVC;
    std::vector<bool> fragmentMap;
    std::vector<uint16_t> fragmentSizes; // Store individual fragment sizes for FEC

    void Reset();
};

struct FECGroup {
    Protocol::FECHeader fecHeader;
    bool fecReceived = false;
    std::unique_ptr<PacketPool::Packet> fecPacket;
    uint32_t frameId = 0;
};

class Receiver {
public:
    Receiver(size_t poolSize = 30);
    void ProcessPacket(const Protocol::VideoHeader& header, const uint8_t* payload);
    void ProcessFEC(const Protocol::FECHeader& header, const uint8_t* payload);

    struct FrameDeleter {
        Receiver* receiver = nullptr;
        void operator()(FrameData* frame) const;
    };
    using FramePtr = std::unique_ptr<FrameData, FrameDeleter>;
    FramePtr GetNextFrame();
    uint32_t GetNextExpectedFrameId() const { return m_nextFrameIdToRead; }
    uint32_t FindLatestAvailableKeyframe();
    void ForceAdvanceTo(uint32_t frameId);
    void ReturnToPool(FramePtr frame);
    void ReturnToPoolRaw(FrameData* frame);

    // Replay protection
    bool ValidateSequence(uint64_t sequenceNumber);

private:
    FrameData* GetFromPoolInternalRaw();
    void ReturnToPoolInternalRaw(FrameData* frame);
    void TryRecover(uint32_t frameId, uint16_t groupStart);

    FixedRingBuffer<FrameData*, 64> m_frameRing;
    std::vector<FrameData*> m_framePool;
    std::vector<std::unique_ptr<FrameData>> m_frameStorage;

    // Use a larger ring and better index to avoid collisions
    FixedRingBuffer<FECGroup, 1024> m_fecRing;
    PacketPool m_fecPool{512, 1500};

    uint32_t m_lastCompletedFrameId = 0;
    uint32_t m_nextFrameIdToRead = 0;
    bool m_firstFrameReceived = false;

    // Sliding window replay protection
    uint64_t m_maxSequenceReceived = 0;
    std::vector<uint64_t> m_sequenceWindow; // Bitmask-based window
    const size_t WINDOW_SIZE = 1024;

    std::mutex m_mutex;
};

} // namespace Client
