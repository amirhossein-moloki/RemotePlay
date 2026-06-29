#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include <memory>
#include <thread>
#include <chrono>

#include "common/protocol.hpp"
#include "common/network_manager.hpp"
#include "common/network_manager.cpp"
#include "common/fixed_ring_buffer.hpp"
#include "common/packet_pool.hpp"
#include "common/safe_queue.hpp"
#include "client/receiver.hpp"
#include "client/receiver.cpp"
#include "client/jitter_buffer.hpp"
#include "client/jitter_buffer.cpp"

void TestProtocol() {
    std::cout << "Running Protocol Tests..." << std::endl;
    assert(sizeof(Protocol::VideoHeader) == 40);
    std::cout << "Protocol Tests Passed!" << std::endl;
}

void TestRingBuffer() {
    std::cout << "Running RingBuffer Tests..." << std::endl;
    FixedRingBuffer<int, 4> rb;
    rb.insert(0, 10);
    assert(*rb.get(0) == 10);
    std::cout << "RingBuffer Tests Passed!" << std::endl;
}

void TestSafeQueue() {
    std::cout << "Running SafeQueue Tests..." << std::endl;
    SafeQueue<int> q;
    q.push(1);
    q.push(2);
    int val;
    assert(q.wait_and_pop(val));
    assert(val == 1);
    assert(q.wait_and_pop(val));
    assert(val == 2);

    std::thread t([&q]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        q.push(3);
    });
    assert(q.wait_and_pop(val));
    assert(val == 3);
    t.join();

    std::thread t2([&q]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        q.stop();
    });
    assert(!q.wait_and_pop(val));
    t2.join();

    std::cout << "SafeQueue Tests Passed!" << std::endl;
}

void TestPacketPool() {
    std::cout << "Running PacketPool Tests..." << std::endl;
    PacketPool pool(2, 100);
    auto p1 = pool.acquire();
    auto p2 = pool.acquire();
    assert(p1->data.size() == 100);

    // Test exhaustion (should return nullptr now)
    auto p3 = pool.acquire();
    assert(p3 == nullptr);

    pool.release(std::move(p1));
    auto p4 = pool.acquire();
    assert(p4->data.size() == 100);
    std::cout << "PacketPool Tests Passed!" << std::endl;
}

void TestReceiverFEC() {
    std::cout << "Running Receiver FEC Tests..." << std::endl;
    Client::Receiver receiver(10);

    Client::Receiver::FrameDeleter noopDeleter;

    uint32_t frameId = 100;
    uint16_t totalFrags = 3;
    uint8_t data0[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
    uint8_t data1[5] = {0, 9, 8, 7, 6}; // Smaller fragment
    uint8_t data2[10] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

    Protocol::VideoHeader vh;
    vh.type = (uint8_t)Protocol::PacketType::Video;
    vh.frameId = frameId;
    vh.totalFragments = totalFrags;
    vh.flags = 0x01; // Mark as keyframe to pass Receiver's first-frame check

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

    Client::Receiver::FramePtr frame = receiver.GetNextFrame();
    assert(frame != nullptr);
    assert(frame->isComplete);

    uint8_t* recovered = frame->buffer.data() + 1 * Protocol::MAX_UDP_PAYLOAD;
    for(int i=0; i<5; ++i) assert(recovered[i] == data1[i]);
    for(int i=5; i<10; ++i) assert(recovered[i] == 0); // Padded with 0

    std::cout << "Receiver FEC Tests Passed!" << std::endl;
}

void TestReceiverStrictOrdering() {
    std::cout << "Running Receiver Strict Ordering Tests..." << std::endl;
    Client::Receiver receiver(10);

    Protocol::VideoHeader vh;
    vh.type = (uint8_t)Protocol::PacketType::Video;
    vh.totalFragments = 1;
    vh.fragmentIndex = 0;
    vh.dataSize = 10;
    uint8_t data[10] = {0};

    vh.frameId = 10;
    vh.flags = 0x01; // Keyframe
    receiver.ProcessPacket(vh, data);
    vh.frameId = 12;
    receiver.ProcessPacket(vh, data);

    // Frame 10 should be returned first
    Client::Receiver::FramePtr frame = receiver.GetNextFrame();
    assert(frame != nullptr);
    assert(frame->frameId == 10);

    // Frame 11 is missing, so frame 12 should NOT be returned yet (Head-of-Line blocking)
    frame = receiver.GetNextFrame();
    assert(frame == nullptr);

    // After forcing advance to 12, frame 12 should be returned
    receiver.ForceAdvanceTo(12);
    frame = receiver.GetNextFrame();
    assert(frame != nullptr);
    assert(frame->frameId == 12);

    // Test FindLatestAvailableKeyframe
    vh.frameId = 15;
    vh.flags = 0x01; // Keyframe
    receiver.ProcessPacket(vh, data);
    vh.frameId = 20;
    vh.flags = 0x01; // Keyframe
    receiver.ProcessPacket(vh, data);
    vh.frameId = 18;
    vh.flags = 0x00; // Normal frame
    receiver.ProcessPacket(vh, data);

    uint32_t latestKey = receiver.FindLatestAvailableKeyframe();
    assert(latestKey == 20);

    std::cout << "Receiver Strict Ordering Tests Passed!" << std::endl;
}

void TestGamepadProtocol() {
    std::cout << "Running Gamepad Protocol Tests..." << std::endl;

    // Test GamepadState
    Protocol::GamepadState gp;
    gp.gamepadId = 1;
    gp.buttons = 0x1234;
    gp.leftTrigger = 100;
    gp.rightTrigger = 200;
    gp.thumbLX = -1000;
    gp.thumbLY = 2000;
    gp.thumbRX = -3000;
    gp.thumbRY = 4000;

    assert(sizeof(Protocol::GamepadState) == 13);

    // Test GamepadStatusEvent
    Protocol::GamepadStatusEvent status;
    status.gamepadId = 2;
    status.isConnected = 1;
    assert(sizeof(Protocol::GamepadStatusEvent) == 2);

    std::cout << "Gamepad Protocol Tests Passed!" << std::endl;
}

void TestJitterBuffer() {
    std::cout << "Running JitterBuffer Tests..." << std::endl;
    Client::JitterBuffer jb(3);

    auto f1 = new Client::FrameData(); f1->frameId = 1;
    f1->buffer.resize(100); f1->fragmentMap.resize(1); f1->fragmentSizes.resize(1);
    auto f2 = new Client::FrameData(); f2->frameId = 2;
    f2->buffer.resize(100); f2->fragmentMap.resize(1); f2->fragmentSizes.resize(1);
    auto f3 = new Client::FrameData(); f3->frameId = 3;
    f3->buffer.resize(100); f3->fragmentMap.resize(1); f3->fragmentSizes.resize(1);
    auto f4 = new Client::FrameData(); f4->frameId = 4;
    f4->buffer.resize(100); f4->fragmentMap.resize(1); f4->fragmentSizes.resize(1);

    jb.PushFrame(Client::Receiver::FramePtr(f1, Client::Receiver::FrameDeleter()));
    jb.PushFrame(Client::Receiver::FramePtr(f2, Client::Receiver::FrameDeleter()));
    jb.PushFrame(Client::Receiver::FramePtr(f3, Client::Receiver::FrameDeleter()));

    // Test strict ordering: PopFrame must return 1, not newest (3)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto p1 = jb.PopFrame();
    assert(p1 != nullptr);
    assert(p1->frameId == 1);

    auto p2 = jb.PopFrame();
    assert(p2 != nullptr);
    assert(p2->frameId == 2);

    auto p3 = jb.PopFrame();
    assert(p3 != nullptr);
    assert(p3->frameId == 3);

    jb.PushFrame(Client::Receiver::FramePtr(f4, Client::Receiver::FrameDeleter()));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto p4 = jb.PopFrame();
    assert(p4 != nullptr);
    assert(p4->frameId == 4);

    std::cout << "JitterBuffer Tests Passed!" << std::endl;
}

void TestHandshakeProtocol() {
    std::cout << "Running Handshake Protocol Tests..." << std::endl;
    assert(sizeof(Protocol::HandshakePacket) == 33);
    assert(sizeof(Protocol::HandshakeResponsePacket) == 2);

    Protocol::HandshakePacket hp;
    hp.type = (uint8_t)Protocol::PacketType::Handshake;
    strcpy(hp.username, "TestUser");
    assert(strcmp(hp.username, "TestUser") == 0);

    Protocol::HandshakeResponsePacket hrp;
    hrp.type = (uint8_t)Protocol::PacketType::HandshakeResponse;
    hrp.approved = 1;
    assert(hrp.approved == 1);

    std::cout << "Handshake Protocol Tests Passed!" << std::endl;
}

int main() {
    try {
        TestProtocol();
        TestRingBuffer();
        TestSafeQueue();
        TestPacketPool();
        TestReceiverFEC();
        TestReceiverStrictOrdering();
        TestJitterBuffer();
        TestGamepadProtocol();
        TestHandshakeProtocol();
        std::cout << "\nAll Core Logic Tests Passed Successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
