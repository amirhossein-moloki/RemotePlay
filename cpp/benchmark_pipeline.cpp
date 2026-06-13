#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <random>

#include "common/protocol.hpp"
#include "common/safe_queue.hpp"
#include "common/packet_pool.hpp"
#include "common/profiler.hpp"
#include "client/receiver.hpp"

// Forward declaration or include needed implementations for the benchmark
// In a real build system these would be linked, but for a standalone benchmark
// we include the necessary source files or rely on them being compiled together.
#include "client/receiver.cpp"
#include "common/network_manager.cpp"

namespace Host {
    struct EncodedPacket {
        std::unique_ptr<PacketPool::Packet> packet;
        uint64_t timestamp;
        bool isKeyframe;
    };
}

struct OutgoingPacket {
    std::unique_ptr<PacketPool::Packet> packet;
    std::string targetIp;
    uint16_t targetPort;
};

void RunBenchmark(int numFrames, int bitrateKbps) {
    std::cout << "Starting Headless Benchmark: " << numFrames << " frames at " << bitrateKbps << " kbps" << std::endl;

    std::atomic<bool> running{true};
    PacketPool packetPool{100, 1024 * 1024};
    PacketPool udpPool{1000, 1500};

    SafeQueue<std::vector<Host::EncodedPacket>> encodeQueue{"EncodeBench"};
    SafeQueue<OutgoingPacket> sendQueue{"SendBench"};

    Client::Receiver receiver(100);

    std::atomic<uint32_t> globalPacketSequence{0};
    std::atomic<int> framesProcessed{0};

    // 5. Mock Input Thread: Generates input events at 100Hz
    std::thread inputThread([&]() {
        while (running) {
            auto udpPkt = udpPool.acquire();
            Protocol::InputHeader* ih = (Protocol::InputHeader*)udpPkt->data.data();
            ih->type = (uint8_t)Protocol::PacketType::Input;
            ih->inputType = (uint8_t)Protocol::InputType::Keyboard;
            ih->timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            udpPkt->size = sizeof(Protocol::InputHeader) + sizeof(Protocol::KeyboardEvent);

            sendQueue.push({std::move(udpPkt), "127.0.0.1", 5005});
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::thread encoderThread([&]() {
        auto frameInterval = std::chrono::microseconds(1000000 / 60);
        for (int i = 0; i < numFrames; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            size_t frameSize = (bitrateKbps * 1000 / 8) / 60;
            std::vector<Host::EncodedPacket> packets;
            Host::EncodedPacket ep;
            ep.packet = packetPool.acquire();
            ep.packet->size = frameSize;
            ep.packet->frameId = i;
            ep.isKeyframe = (i % 60 == 0);
            ep.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count();
            packets.push_back(std::move(ep));
            encodeQueue.push(std::move(packets));
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            if (elapsed < frameInterval) {
                std::this_thread::sleep_for(frameInterval - elapsed);
            }
        }
    });

    std::thread packetizerThread([&]() {
        std::vector<Host::EncodedPacket> frames;
        while (running && encodeQueue.wait_and_pop(frames)) {
            ScopeTimer timer("Packetizer_Total");
            for (auto& frame : frames) {
                uint16_t totalFrags = (uint16_t)((frame.packet->size + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);
                std::vector<PacketPool::Packet*> fragmentPtrs(totalFrags);
                for (uint16_t i = 0; i < totalFrags; ++i) {
                    auto udpPkt = udpPool.acquire();
                    Protocol::VideoHeader* vh = (Protocol::VideoHeader*)udpPkt->data.data();
                    vh->type = (uint8_t)Protocol::PacketType::Video;
                    vh->frameId = frame.packet->frameId;
                    vh->fragmentIndex = i;
                    vh->totalFragments = totalFrags;
                    vh->packetSequence = globalPacketSequence++;
                    vh->timestamp = frame.timestamp;
                    vh->flags = frame.isKeyframe ? 0x01 : 0x00;
                    vh->dataSize = (uint16_t)std::min((uint32_t)Protocol::MAX_UDP_PAYLOAD, (uint32_t)(frame.packet->size - i * Protocol::MAX_UDP_PAYLOAD));
                    udpPkt->size = sizeof(Protocol::VideoHeader) + vh->dataSize;
                    fragmentPtrs[i] = udpPkt.release();
                }
                for (uint16_t i = 0; i < totalFrags; ++i) {
                    auto sendPkt = udpPool.acquire();
                    sendPkt->size = fragmentPtrs[i]->size;
                    memcpy(sendPkt->data.data(), fragmentPtrs[i]->data.data(), fragmentPtrs[i]->size);
                    sendQueue.push({std::move(sendPkt), "127.0.0.1", 5005});
                    const int FEC_GROUP_SIZE = 5;
                    if ((i + 1) % FEC_GROUP_SIZE == 0 || i == totalFrags - 1) {
                        size_t groupStart = (i / FEC_GROUP_SIZE) * FEC_GROUP_SIZE;
                        uint16_t count = (uint16_t)(i - groupStart + 1);
                        if (count > 1) {
                            auto fecPkt = udpPool.acquire();
                            Protocol::FECHeader* fh = (Protocol::FECHeader*)fecPkt->data.data();
                            fh->type = (uint8_t)Protocol::PacketType::FEC;
                            fh->frameId = frame.packet->frameId;
                            fh->fragmentStart = (uint16_t)groupStart;
                            fh->fragmentCount = count;
                            uint16_t maxPayloadSize = 0;
                            for (size_t j = 0; j < count; ++j) {
                                maxPayloadSize = std::max(maxPayloadSize, ((Protocol::VideoHeader*)fragmentPtrs[groupStart+j]->data.data())->dataSize);
                            }
                            fh->dataSize = maxPayloadSize;
                            fecPkt->size = sizeof(Protocol::FECHeader) + fh->dataSize;
                            sendQueue.push({std::move(fecPkt), "127.0.0.1", 5005});
                        }
                    }
                }
                for (size_t i = 0; i < totalFrags; ++i) {
                    udpPool.release(std::unique_ptr<PacketPool::Packet>(fragmentPtrs[i]));
                }
                packetPool.release(std::move(frame.packet));
            }
        }
    });

    std::thread networkThread([&]() {
        OutgoingPacket op;
        while (running && sendQueue.wait_and_pop(op)) {
            Protocol::PacketType type = (Protocol::PacketType)op.packet->data[0];
            if (type == Protocol::PacketType::Video) {
                Protocol::VideoHeader* vh = (Protocol::VideoHeader*)op.packet->data.data();
                receiver.ProcessPacket(*vh, op.packet->data.data() + sizeof(Protocol::VideoHeader));
            } else if (type == Protocol::PacketType::FEC) {
                Protocol::FECHeader* fh = (Protocol::FECHeader*)op.packet->data.data();
                receiver.ProcessFEC(*fh, op.packet->data.data() + sizeof(Protocol::FECHeader));
            } else if (type == Protocol::PacketType::Input) {
                Protocol::InputHeader* ih = (Protocol::InputHeader*)op.packet->data.data();
                uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                Profiler::getInstance().recordTime("Input_NetworkLatency", (double)(now - ih->timestamp));
                Profiler::getInstance().recordCounter("Input_Packets");
            }
            udpPool.release(std::move(op.packet));
        }
    });

    std::thread clientThread([&]() {
        while (running) {
            auto frame = receiver.GetNextFrame();
            if (frame) {
                framesProcessed++;
                receiver.ReturnToPool(std::move(frame));
                if (framesProcessed >= numFrames) running = false;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    if (encoderThread.joinable()) encoderThread.join();
    if (inputThread.joinable()) inputThread.join();
    auto startWait = std::chrono::steady_clock::now();
    while (running && std::chrono::steady_clock::now() - startWait < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    running = false;
    encodeQueue.stop();
    sendQueue.stop();
    if (packetizerThread.joinable()) packetizerThread.join();
    if (networkThread.joinable()) networkThread.join();
    if (clientThread.joinable()) clientThread.join();
    Profiler::getInstance().report();
}

int main() {
    RunBenchmark(600, 5000);   // 10s at 5Mbps
    Profiler::getInstance().reset();
    RunBenchmark(600, 20000);  // 10s at 20Mbps
    return 0;
}
