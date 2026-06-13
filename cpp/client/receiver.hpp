#pragma once

#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <cstring>
#include "../common/protocol.hpp"

namespace Client {

struct FrameData {
    uint32_t sequence;
    std::vector<uint8_t> buffer;
    size_t totalSize;
    uint16_t fragmentsReceived;
    uint16_t totalFragments;
    uint64_t timestamp;
    bool isComplete;

    void Reset();
};

class Receiver {
public:
    Receiver(size_t poolSize = 10);
    void ProcessPacket(const Protocol::VideoHeader& header, const uint8_t* payload);
    std::unique_ptr<FrameData> GetNextFrame();
    void ReturnToPool(std::unique_ptr<FrameData> frame);

private:
    std::unique_ptr<FrameData> GetFromPoolInternal();
    void ReturnToPoolInternal(std::unique_ptr<FrameData> frame);

    std::map<uint32_t, std::unique_ptr<FrameData>> m_pendingFrames;
    std::vector<std::unique_ptr<FrameData>> m_framePool;
    uint32_t m_lastCompletedSequence = 0;
    std::mutex m_mutex;
};

} // namespace Client
