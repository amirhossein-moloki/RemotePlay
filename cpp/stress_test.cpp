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

#include "client/receiver.cpp"
#include "common/network_manager.cpp"

namespace Host {
    struct EncodedPacket {
        std::unique_ptr<PacketPool::Packet> packet;
        bool isKeyframe;
        uint64_t captureTimestamp;
        uint64_t encodeStartTimestamp;
        uint64_t encodeEndTimestamp;
    };
}

struct OutgoingPacket {
    std::unique_ptr<PacketPool::Packet> packet;
    std::string targetIp;
    uint16_t targetPort;
};

void RunStressTest(int numFrames, float lossRate) {
    std::cout << "Starting Reliability Stress Test: " << numFrames << " frames, " << (lossRate * 100) << "% loss" << std::endl;

    std::atomic<bool> running{true};
    PacketPool packetPool{200, 1024 * 1024};
    PacketPool udpPool{2000, 1500};

    SafeQueue<std::vector<Host::EncodedPacket>> encodeQueue("EncodeStress");
    SafeQueue<OutgoingPacket> sendQueue("SendStress");

    Client::Receiver receiver(200);

    std::atomic<uint32_t> globalPacketSequence{0};
    std::atomic<int> framesProcessed{0};
    std::atomic<int> framesLost{0};

    std::mt19937 gen(1337);
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    std::thread encoderThread([&]() {
        auto frameInterval = std::chrono::microseconds(1000000 / 60);
        for (int i = 0; i < numFrames; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            size_t frameSize = 10000;
            std::vector<Host::EncodedPacket> packets;
            Host::EncodedPacket ep;
            ep.packet = packetPool.acquire();
            if (!ep.packet) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); i--; continue; }
            ep.packet->size = frameSize;
            ep.packet->frameId = i;
            ep.isKeyframe = (i % 60 == 0);
            ep.captureTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count();
            ep.encodeStartTimestamp = ep.captureTimestamp + 1000;
            ep.encodeEndTimestamp = ep.encodeStartTimestamp + 2000;
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
            for (auto& frame : frames) {
                uint16_t totalFrags = (uint16_t)((frame.packet->size + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);
                std::vector<PacketPool::Packet*> fragmentPtrs(totalFrags);
                for (uint16_t i = 0; i < totalFrags; ++i) {
                    auto udpPkt = udpPool.acquire();
                    if (!udpPkt) continue;
                    Protocol::VideoHeader* vh = (Protocol::VideoHeader*)udpPkt->data.data();
                    vh->type = (uint8_t)Protocol::PacketType::Video;
                    vh->frameId = frame.packet->frameId;
                    vh->fragmentIndex = i;
                    vh->totalFragments = totalFrags;
                    vh->packetSequence = globalPacketSequence++;
                    vh->captureTimestamp = frame.captureTimestamp;
                    vh->encodeStartTimestamp = frame.encodeStartTimestamp;
                    vh->encodeEndTimestamp = frame.encodeEndTimestamp;
                    vh->flags = frame.isKeyframe ? 0x01 : 0x00;
                    vh->dataSize = (uint16_t)std::min((uint32_t)Protocol::MAX_UDP_PAYLOAD, (uint32_t)(frame.packet->size - i * Protocol::MAX_UDP_PAYLOAD));
                    udpPkt->size = sizeof(Protocol::VideoHeader) + vh->dataSize;
                    fragmentPtrs[i] = udpPkt.release();
                }

                for (uint16_t i = 0; i < totalFrags; ++i) {
                    if (!fragmentPtrs[i]) continue;
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

                            uint8_t* fecPayload = fecPkt->data.data() + sizeof(Protocol::FECHeader);
                            memset(fecPayload, 0, fh->dataSize);
                            for (size_t j = 0; j < count; ++j) {
                                uint8_t* fragData = fragmentPtrs[groupStart+j]->data.data() + sizeof(Protocol::VideoHeader);
                                uint16_t fragSize = ((Protocol::VideoHeader*)fragmentPtrs[groupStart+j]->data.data())->dataSize;
                                for (uint16_t k = 0; k < fragSize; ++k) fecPayload[k] ^= fragData[k];
                            }

                            fecPkt->size = sizeof(Protocol::FECHeader) + fh->dataSize;
                            sendQueue.push({std::move(fecPkt), "127.0.0.1", 5005});
                        }
                    }
                }
                for(auto p : fragmentPtrs) if(p) { std::unique_ptr<PacketPool::Packet> up(p); udpPool.release(std::move(up)); }
                packetPool.release(std::move(frame.packet));
            }
        }
    });

    std::thread networkThread([&]() {
        OutgoingPacket op;
        while (running && sendQueue.wait_and_pop(op)) {
            if (dis(gen) < lossRate) {
                udpPool.release(std::move(op.packet));
                continue;
            }

            Protocol::PacketType type = (Protocol::PacketType)op.packet->data[0];
            if (type == Protocol::PacketType::Video) {
                Protocol::VideoHeader* vh = (Protocol::VideoHeader*)op.packet->data.data();
                receiver.ProcessPacket(*vh, op.packet->data.data() + sizeof(Protocol::VideoHeader));
            } else if (type == Protocol::PacketType::FEC) {
                Protocol::FECHeader* fh = (Protocol::FECHeader*)op.packet->data.data();
                receiver.ProcessFEC(*fh, op.packet->data.data() + sizeof(Protocol::FECHeader));
            }
            udpPool.release(std::move(op.packet));
        }
    });

    std::thread clientThread([&]() {
        while (running) {
            auto frame = receiver.GetNextFrame();
            if (frame) {
                if (frame->isComplete) framesProcessed++;
                else framesLost++;
                receiver.ReturnToPool(std::move(frame));
                if (framesProcessed + framesLost >= numFrames) running = false;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    encoderThread.join();
    auto startWait = std::chrono::steady_clock::now();
    while (running && std::chrono::steady_clock::now() - startWait < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    running = false;
    encodeQueue.stop();
    sendQueue.stop();
    packetizerThread.join();
    networkThread.join();
    clientThread.join();

    std::cout << "Results for " << (lossRate * 100) << "% loss:" << std::endl;
    std::cout << "  Complete: " << framesProcessed << " | Lost: " << framesLost << std::endl;
}

int main() {
    RunStressTest(100, 0.01f);
    RunStressTest(100, 0.05f);
    RunStressTest(100, 0.10f);
    return 0;
}
