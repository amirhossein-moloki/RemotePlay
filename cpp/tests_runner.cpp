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

    // Test fallback
    auto p3 = pool.acquire();
    assert(p3->data.size() == 1024 * 1024);

    pool.release(std::move(p1));
    auto p4 = pool.acquire();
    assert(p4->data.size() == 100);
    std::cout << "PacketPool Tests Passed!" << std::endl;
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

void TestJitterBuffer() {
    std::cout << "Running JitterBuffer Tests..." << std::endl;
    Client::JitterBuffer jb(3);

    auto f1 = std::make_unique<Client::FrameData>(); f1->frameId = 1;
    f1->buffer.resize(100); f1->fragmentMap.resize(1); f1->fragmentSizes.resize(1);
    auto f2 = std::make_unique<Client::FrameData>(); f2->frameId = 2;
    f2->buffer.resize(100); f2->fragmentMap.resize(1); f2->fragmentSizes.resize(1);
    auto f3 = std::make_unique<Client::FrameData>(); f3->frameId = 3;
    f3->buffer.resize(100); f3->fragmentMap.resize(1); f3->fragmentSizes.resize(1);
    auto f4 = std::make_unique<Client::FrameData>(); f4->frameId = 4;
    f4->buffer.resize(100); f4->fragmentMap.resize(1); f4->fragmentSizes.resize(1);

    jb.PushFrame(std::move(f1));
    jb.PushFrame(std::move(f2));
    jb.PushFrame(std::move(f3));

    // Test timing pop - wait for 10ms threshold
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto p1 = jb.PopFrame();
    assert(p1 != nullptr);
    assert(p1->frameId == 3); // Current PopFrame pops newest available if threshold met

    jb.PushFrame(std::move(f4));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto p2 = jb.PopFrame();
    assert(p2 != nullptr);
    assert(p2->frameId == 4);

    std::cout << "JitterBuffer Tests Passed!" << std::endl;
}

int main() {
    try {
        TestProtocol();
        TestRingBuffer();
        TestSafeQueue();
        TestPacketPool();
        TestReceiverFEC();
        TestReceiverSkipping();
        TestJitterBuffer();
        std::cout << "\nAll Core Logic Tests Passed Successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
