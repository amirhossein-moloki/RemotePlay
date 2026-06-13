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

void TestJitterBuffer() {
    std::cout << "Running JitterBuffer Tests (Isolated)..." << std::endl;
    Client::JitterBuffer jb(3);
    std::cout << "Created JB" << std::endl;

    auto f1 = std::make_unique<Client::FrameData>(); f1->frameId = 1;
    f1->buffer.resize(100);
    f1->fragmentMap.resize(1);
    f1->fragmentSizes.resize(1);
    std::cout << "Created f1" << std::endl;

    jb.PushFrame(std::move(f1));
    std::cout << "Pushed f1" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto p1 = jb.PopFrame();
    std::cout << "Popped p1" << std::endl;
    assert(p1 != nullptr);
    assert(p1->frameId == 1);

    std::cout << "Isolated JitterBuffer Test Passed!" << std::endl;
}

int main() {
    try {
        TestJitterBuffer();
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
