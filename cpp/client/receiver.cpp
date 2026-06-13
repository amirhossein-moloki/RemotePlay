#include <map>
#include <vector>
#include <mutex>
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
};

class Receiver {
public:
    Receiver() {}

    void ProcessPacket(const Protocol::VideoHeader& header, const uint8_t* payload) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Drop old frames
        if (header.sequence < m_lastCompletedSequence && m_lastCompletedSequence != 0) {
            return;
        }

        auto& frame = m_pendingFrames[header.sequence];
        if (frame.buffer.empty()) {
            frame.sequence = header.sequence;
            frame.totalFragments = header.totalFragments;
            frame.fragmentsReceived = 0;
            frame.timestamp = header.timestamp;
            frame.isComplete = false;
            // Rough estimate of size
            frame.buffer.resize(header.totalFragments * Protocol::MAX_UDP_PAYLOAD);
        }

        // Copy fragment to buffer
        size_t offset = header.fragmentIndex * Protocol::MAX_UDP_PAYLOAD;
        if (offset + header.dataSize <= frame.buffer.size()) {
            memcpy(frame.buffer.data() + offset, payload, header.dataSize);
            frame.fragmentsReceived++;
            if (header.fragmentIndex == header.totalFragments - 1) {
                frame.totalSize = offset + header.dataSize;
            }
        }

        if (frame.fragmentsReceived == frame.totalFragments) {
            frame.isComplete = true;
            frame.buffer.resize(frame.totalSize);
            m_lastCompletedSequence = header.sequence;
        }
    }

    bool GetNextFrame(FrameData& outFrame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pendingFrames.empty()) return false;

        auto it = m_pendingFrames.begin();
        if (it->second.isComplete) {
            outFrame = std::move(it->second);
            m_pendingFrames.erase(it);
            return true;
        }

        // Optional: If we have many pending frames, drop incomplete ones that are too old
        if (m_pendingFrames.size() > 5) {
             m_pendingFrames.erase(it);
        }

        return false;
    }

private:
    std::map<uint32_t, FrameData> m_pendingFrames;
    uint32_t m_lastCompletedSequence = 0;
    std::mutex m_mutex;
};

} // namespace Client
