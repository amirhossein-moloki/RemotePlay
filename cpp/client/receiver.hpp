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
    uint64_t timestamp;
    bool isComplete;
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
    void ReturnToPool(FramePtr frame);
    void ReturnToPoolRaw(FrameData* frame);

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
    std::mutex m_mutex;
};

} // namespace Client
