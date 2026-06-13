#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include <memory>
#include "common/protocol.hpp"
#include "common/network_manager.hpp"
#include "common/network_manager.cpp"
#include "common/fixed_ring_buffer.hpp"
#include "common/packet_pool.hpp"
#include "client/receiver.hpp"
#include "client/receiver.cpp"
#include "client/jitter_buffer.hpp"
#include "client/jitter_buffer.cpp"

void TestProtocol() {
    std::cout << "Running Protocol Tests..." << std::endl;
    assert(sizeof(Protocol::VideoHeader) == 24);
    std::cout << "Protocol Tests Passed!" << std::endl;
}

void TestRingBuffer() {
    std::cout << "Running RingBuffer Tests..." << std::endl;
    FixedRingBuffer<int, 4> rb;
    rb.insert(0, 10);
    assert(*rb.get(0) == 10);
    std::cout << "RingBuffer Tests Passed!" << std::endl;
}

void TestReceiverFEC() {
    std::cout << "Running Receiver FEC Tests..." << std::endl;
    Client::Receiver receiver(10);

    uint32_t frameId = 100;
    uint16_t totalFrags = 3;
    uint8_t data0[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
    uint8_t data1[5] = {0, 9, 8, 7, 6}; // Smaller fragment
    uint8_t data2[10] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

    Protocol::VideoHeader vh;
    vh.type = (uint8_t)Protocol::PacketType::Video;
    vh.frameId = frameId;
    vh.totalFragments = totalFrags;

    // Send frag 0, skip 1, send frag 2
    vh.fragmentIndex = 0; vh.dataSize = 10;
    receiver.ProcessPacket(vh, data0);
    vh.fragmentIndex = 2; vh.dataSize = 10;
    receiver.ProcessPacket(vh, data2);

    // Send FEC for group 0-3. dataSize should be 10 (max of group)
    Protocol::FECHeader fh;
    fh.type = (uint8_t)Protocol::PacketType::FEC;
    fh.frameId = frameId;
    fh.fragmentStart = 0;
    fh.fragmentCount = 3;
    fh.dataSize = 10;

    uint8_t fecData[10];
    for(int i=0; i<10; ++i) {
        uint8_t b0 = data0[i];
        uint8_t b1 = (i < 5) ? data1[i] : 0;
        uint8_t b2 = data2[i];
        fecData[i] = b0 ^ b1 ^ b2;
    }

    receiver.ProcessFEC(fh, fecData);

    auto frame = receiver.GetNextFrame();
    assert(frame != nullptr);
    assert(frame->isComplete);

    uint8_t* recovered = frame->buffer.data() + 1 * Protocol::MAX_UDP_PAYLOAD;
    for(int i=0; i<5; ++i) assert(recovered[i] == data1[i]);
    for(int i=5; i<10; ++i) assert(recovered[i] == 0); // Padded with 0

    std::cout << "Receiver FEC Tests Passed!" << std::endl;
}

void TestReceiverSkipping() {
    std::cout << "Running Receiver Skipping Tests..." << std::endl;
    Client::Receiver receiver(10);

    Protocol::VideoHeader vh;
    vh.type = (uint8_t)Protocol::PacketType::Video;
    vh.totalFragments = 1;
    vh.fragmentIndex = 0;
    vh.dataSize = 10;
    uint8_t data[10] = {0};

    vh.frameId = 10;
    receiver.ProcessPacket(vh, data);
    vh.frameId = 12;
    receiver.ProcessPacket(vh, data);

    auto frame = receiver.GetNextFrame();
    assert(frame != nullptr);
    assert(frame->frameId == 12);

    std::cout << "Receiver Skipping Tests Passed!" << std::endl;
}

int main() {
    try {
        TestProtocol();
        TestRingBuffer();
        TestReceiverFEC();
        TestReceiverSkipping();
        std::cout << "\nAll Hardening Tests Passed Successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
