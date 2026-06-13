#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>
#include <thread>
#include "common/protocol.hpp"
#include "common/packet_pool.hpp"
#include "client/receiver.hpp"
#include "client/receiver.cpp"
#include "client/jitter_buffer.hpp"
#include "client/jitter_buffer.cpp"
#include "common/network_manager.cpp"

/**
 * Headless end-to-end logic test.
 * Simulates:
 * 1. Frame fragmentation (Host side logic)
 * 2. Packet reception (Client side logic)
 * 3. FEC recovery
 * 4. Jitter buffer reordering/timing
 */

void TestEndToEndLogic() {
    std::cout << "Starting Headless End-to-End Logic Test..." << std::endl;

    Client::Receiver receiver(10);
    Client::JitterBuffer jitterBuffer(5);
    PacketPool pool(100, 1500);

    uint32_t frameId = 42;
    size_t originalSize = 5000;
    std::vector<uint8_t> originalData(originalSize);
    for (size_t i = 0; i < originalSize; ++i) originalData[i] = (uint8_t)(i % 256);

    // 1. Fragment the frame (Simulating Host packetizer)
    uint16_t totalFrags = (uint16_t)((originalSize + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);
    std::cout << "Fragmenting " << originalSize << " bytes into " << totalFrags << " fragments." << std::endl;

    // Simulate losing fragment 1 (index 1) to test FEC
    // We need at least 2 fragments to use FEC in our implementation (FEC_GROUP_SIZE=5)

    std::vector<std::vector<uint8_t>> udpPackets;
    for (uint16_t i = 0; i < totalFrags; ++i) {
        Protocol::VideoHeader vh;
        vh.type = (uint8_t)Protocol::PacketType::Video;
        vh.frameId = frameId;
        vh.fragmentIndex = i;
        vh.totalFragments = totalFrags;
        vh.dataSize = (uint16_t)std::min((size_t)Protocol::MAX_UDP_PAYLOAD, originalSize - i * Protocol::MAX_UDP_PAYLOAD);

        std::vector<uint8_t> pkt(sizeof(Protocol::VideoHeader) + vh.dataSize);
        memcpy(pkt.data(), &vh, sizeof(vh));
        memcpy(pkt.data() + sizeof(vh), originalData.data() + i * Protocol::MAX_UDP_PAYLOAD, vh.dataSize);
        udpPackets.push_back(pkt);
    }

    // Create FEC for the group
    Protocol::FECHeader fh;
    fh.type = (uint8_t)Protocol::PacketType::FEC;
    fh.frameId = frameId;
    fh.fragmentStart = 0;
    fh.fragmentCount = totalFrags;
    fh.dataSize = Protocol::MAX_UDP_PAYLOAD;

    std::vector<uint8_t> fecPayload(fh.dataSize, 0);
    for (uint16_t i = 0; i < totalFrags; ++i) {
        Protocol::VideoHeader* vh = (Protocol::VideoHeader*)udpPackets[i].data();
        uint8_t* payload = udpPackets[i].data() + sizeof(Protocol::VideoHeader);
        for (size_t k = 0; k < vh->dataSize; ++k) fecPayload[k] ^= payload[k];
    }
    std::vector<uint8_t> fecPkt(sizeof(Protocol::FECHeader) + fh.dataSize);
    memcpy(fecPkt.data(), &fh, sizeof(fh));
    memcpy(fecPkt.data() + sizeof(fh), fecPayload.data(), fh.dataSize);

    // 2. Process packets at Receiver (Simulating Client)
    // Send all but fragment 1
    for (uint16_t i = 0; i < totalFrags; ++i) {
        if (i == 1) continue;
        Protocol::VideoHeader* vh = (Protocol::VideoHeader*)udpPackets[i].data();
        receiver.ProcessPacket(*vh, udpPackets[i].data() + sizeof(Protocol::VideoHeader));
    }

    // Frame should not be complete yet
    assert(receiver.GetNextFrame() == nullptr);
    std::cout << "Frame incomplete as expected (missing frag 1)." << std::endl;

    // Send FEC
    receiver.ProcessFEC(fh, fecPkt.data() + sizeof(Protocol::FECHeader));
    std::cout << "Processed FEC packet." << std::endl;

    // 3. Verify Recovery
    auto frame = receiver.GetNextFrame();
    assert(frame != nullptr);
    assert(frame->frameId == frameId);
    assert(frame->isComplete);

    // Verify data integrity
    for (size_t i = 0; i < originalSize; ++i) {
        if (frame->buffer[i] != originalData[i]) {
            std::cerr << "Data mismatch at byte " << i << " Expected: " << (int)originalData[i] << " Got: " << (int)frame->buffer[i] << std::endl;
            assert(false);
        }
    }
    std::cout << "Data recovery successful and verified!" << std::endl;

    // 4. Jitter Buffer flow
    jitterBuffer.PushFrame(std::move(frame));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto poppedFrame = jitterBuffer.PopFrame();
    assert(poppedFrame != nullptr);
    assert(poppedFrame->frameId == frameId);

    std::cout << "Headless End-to-End Logic Test Passed!" << std::endl;
}

int main() {
    try {
        TestEndToEndLogic();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
