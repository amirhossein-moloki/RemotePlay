#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include "common/protocol.hpp"
#include "common/network_manager.hpp"
#include "common/network_manager.cpp"

void TestProtocol() {
    std::cout << "Running Protocol Tests..." << std::endl;

    // Test VideoHeader size and alignment
    assert(sizeof(Protocol::VideoHeader) == 20); // 1 + 4 + 2 + 2 + 8 + 1 + 2 = 20

    Protocol::VideoHeader header;
    header.type = (uint8_t)Protocol::PacketType::Video;
    header.sequence = 1234;
    header.fragmentIndex = 5;
    header.totalFragments = 10;
    header.timestamp = 987654321;
    header.flags = 0x01;
    header.dataSize = 1000;

    std::vector<uint8_t> buffer(sizeof(header));
    memcpy(buffer.data(), &header, sizeof(header));

    Protocol::VideoHeader decoded;
    memcpy(&decoded, buffer.data(), sizeof(decoded));

    assert(decoded.type == (uint8_t)Protocol::PacketType::Video);
    assert(decoded.sequence == 1234);
    assert(decoded.fragmentIndex == 5);
    assert(decoded.totalFragments == 10);
    assert(decoded.timestamp == 987654321);
    assert(decoded.flags == 0x01);
    assert(decoded.dataSize == 1000);

    std::cout << "Protocol Tests Passed!" << std::endl;
}

void TestNetworkManager() {
    std::cout << "Running NetworkManager Tests..." << std::endl;

    auto interfaces = Network::NetworkManager::EnumerateInterfaces();
    assert(!interfaces.empty());

    std::cout << "Found " << interfaces.size() << " interfaces." << std::endl;
    for (const auto& iface : interfaces) {
        std::cout << " - " << iface.name << ": " << iface.ip << std::endl;
    }

    std::cout << "NetworkManager Tests Passed!" << std::endl;
}

int main() {
    try {
        TestProtocol();
        TestNetworkManager();
        std::cout << "\nAll Tests Passed Successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
