#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <set>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <array>

#include "common/network_manager.hpp"
#include "common/protocol.hpp"
#include "common/safe_queue.hpp"
#include "common/packet_pool.hpp"
#include "common/fixed_ring_buffer.hpp"

#ifdef _WIN32
#include <windows.h>
#include "host/capture_dxgi.hpp"
#include "host/encoder_hw.hpp"
#include "host/input_injector.hpp"
#include "client/receiver.hpp"
#include "client/decoder_hw.hpp"
#include "client/input_capture.hpp"
#include "client/renderer_d3d11.hpp"
#include "client/jitter_buffer.hpp"
#endif

void ShowUsage() {
    std::cout << "Usage: parsec-lite.exe [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --host           Start in Host Mode" << std::endl;
    std::cout << "  --client <IP>    Start in Client Mode connecting to <IP>" << std::endl;
    std::cout << "  --list           List available network interfaces" << std::endl;
    std::cout << "  --iface <index>  Explicitly select interface by index from --list" << std::endl;
}

#ifdef _WIN32

struct OutgoingPacket {
    std::unique_ptr<PacketPool::Packet> packet;
    std::string targetIp;
    uint16_t targetPort;
};

struct HostContext {
    std::atomic<bool> running{true};
    Network::NetworkManager net;
    Host::CaptureDXGI capture;
    Host::FFmpegHardwareEncoder encoder;
    Host::InputInjector injector;
    std::mutex clientsMutex;
    std::set<std::pair<std::string, uint16_t>> clients;

    SafeQueue<ID3D11Texture2D*> captureQueue;
    SafeQueue<std::vector<Host::EncodedPacket>> encodeQueue;
    SafeQueue<OutgoingPacket> sendQueue;

    PacketPool packetPool{200, 1024 * 1024};
    PacketPool udpPool{1000, 1500};

    std::atomic<int> currentBitrate{5000};
    std::atomic<uint32_t> globalPacketSequence{0};
};

void RunHost(const std::string& ip) {
    HostContext ctx;
    if (!ctx.net.Bind(ip, 5005)) return;
    if (!ctx.capture.Initialize()) return;
    if (!ctx.encoder.Initialize(1920, 1080, 60, ctx.currentBitrate)) return;

    std::thread captureThread([&]() {
        while (ctx.running) {
            ID3D11Texture2D* tex = nullptr;
            if (ctx.capture.AcquireFrame(&tex)) {
                ctx.captureQueue.push(tex);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::thread encoderThread([&]() {
        ID3D11Texture2D* tex = nullptr;
        while (ctx.running && ctx.captureQueue.wait_and_pop(tex)) {
            ctx.encoder.SetBitrate(ctx.currentBitrate);
            std::vector<Host::EncodedPacket> packets;
            if (ctx.encoder.EncodeFrame(tex, packets, ctx.packetPool)) {
                ctx.encodeQueue.push(std::move(packets));
            }
            ctx.capture.ReleaseFrame();
        }
    });

    std::thread packetizerThread([&]() {
        std::vector<Host::EncodedPacket> frames;
        uint32_t frameId = 0;

        while (ctx.running && ctx.encodeQueue.wait_and_pop(frames)) {
            for (auto& frame : frames) {
                uint16_t totalFrags = (uint16_t)((frame.packet->size + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);

                // Use a fixed-size array on stack or pre-allocated to avoid vector allocations
                static thread_local std::array<PacketPool::Packet*, 1024> fragmentPtrs;
                if (totalFrags > 1024) continue;

                for (uint16_t i = 0; i < totalFrags; ++i) {
                    auto udpPkt = ctx.udpPool.acquire();
                    Protocol::VideoHeader* vh = (Protocol::VideoHeader*)udpPkt->data.data();
                    vh->type = (uint8_t)Protocol::PacketType::Video;
                    vh->frameId = frameId;
                    vh->fragmentIndex = i;
                    vh->totalFragments = totalFrags;
                    vh->packetSequence = ctx.globalPacketSequence++;
                    vh->timestamp = frame.timestamp;
                    vh->flags = frame.isKeyframe ? 0x01 : 0x00;
                    vh->dataSize = (uint16_t)std::min((uint32_t)Protocol::MAX_UDP_PAYLOAD, (uint32_t)(frame.packet->size - i * Protocol::MAX_UDP_PAYLOAD));

                    memcpy(udpPkt->data.data() + sizeof(Protocol::VideoHeader),
                           frame.packet->data.data() + (i * Protocol::MAX_UDP_PAYLOAD), vh->dataSize);
                    udpPkt->size = sizeof(Protocol::VideoHeader) + vh->dataSize;
                    fragmentPtrs[i] = udpPkt.release(); // Ownership moved to fragmentPtrs for now
                }

                std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                for (const auto& client : ctx.clients) {
                    for (size_t i = 0; i < totalFrags; ++i) {
                        auto sendPkt = ctx.udpPool.acquire();
                        sendPkt->size = fragmentPtrs[i]->size;
                        memcpy(sendPkt->data.data(), fragmentPtrs[i]->data.data(), fragmentPtrs[i]->size);
                        ctx.sendQueue.push({std::move(sendPkt), client.first, client.second});

                        const int FEC_GROUP_SIZE = 5;
                        if ((i + 1) % FEC_GROUP_SIZE == 0 || i == totalFrags - 1) {
                            size_t groupStart = (i / FEC_GROUP_SIZE) * FEC_GROUP_SIZE;
                            uint16_t count = (uint16_t)(i - groupStart + 1);
                            if (count > 1) {
                                auto fecPkt = ctx.udpPool.acquire();
                                Protocol::FECHeader* fh = (Protocol::FECHeader*)fecPkt->data.data();
                                fh->type = (uint8_t)Protocol::PacketType::FEC;
                                fh->frameId = frameId;
                                fh->fragmentStart = (uint16_t)groupStart;
                                fh->fragmentCount = count;
                                fh->packetSequence = ctx.globalPacketSequence++;

                                uint16_t maxPayloadSize = 0;
                                for (size_t j = 0; j < count; ++j) {
                                    maxPayloadSize = std::max(maxPayloadSize, ((Protocol::VideoHeader*)fragmentPtrs[groupStart+j]->data.data())->dataSize);
                                }
                                fh->dataSize = maxPayloadSize;

                                uint8_t* fecPayload = fecPkt->data.data() + sizeof(Protocol::FECHeader);
                                memset(fecPayload, 0, maxPayloadSize);
                                for (size_t j = 0; j < count; ++j) {
                                    Protocol::VideoHeader* vh = (Protocol::VideoHeader*)fragmentPtrs[groupStart+j]->data.data();
                                    uint8_t* fragPayload = fragmentPtrs[groupStart+j]->data.data() + sizeof(Protocol::VideoHeader);
                                    for (size_t k = 0; k < vh->dataSize; ++k) fecPayload[k] ^= fragPayload[k];
                                }
                                fecPkt->size = sizeof(Protocol::FECHeader) + fh->dataSize;
                                ctx.sendQueue.push({std::move(fecPkt), client.first, client.second});
                            }
                        }
                    }
                }

                for (size_t i = 0; i < totalFrags; ++i) {
                    ctx.udpPool.release(std::unique_ptr<PacketPool::Packet>(fragmentPtrs[i]));
                }
                ctx.packetPool.release(std::move(frame.packet));
            }
            frameId++;
        }
    });

    std::thread senderThread([&]() {
        OutgoingPacket op;
        while (ctx.running && ctx.sendQueue.wait_and_pop(op)) {
            ctx.net.SendTo(op.packet->data.data(), op.packet->size, op.targetIp, op.targetPort);
            ctx.udpPool.release(std::move(op.packet));
        }
    });

    std::thread receiverThread([&]() {
        uint8_t buf[2048];
        std::string senderIp;
        uint16_t senderPort;
        while (ctx.running) {
            int len = ctx.net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            if (len > 0) {
                Protocol::PacketType type = (Protocol::PacketType)buf[0];
                if (type == Protocol::PacketType::Handshake) {
                    std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                    ctx.clients.insert({senderIp, senderPort});
                } else if (type == Protocol::PacketType::Input && len >= (int)sizeof(Protocol::InputHeader)) {
                    Protocol::InputHeader* ih = (Protocol::InputHeader*)buf;
                    if (ih->inputType == (uint8_t)Protocol::InputType::Keyboard && len >= (int)(sizeof(Protocol::InputHeader) + sizeof(Protocol::KeyboardEvent))) {
                        ctx.injector.InjectKeyboard(*(Protocol::KeyboardEvent*)(buf + sizeof(Protocol::InputHeader)));
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseMove && len >= (int)(sizeof(Protocol::InputHeader) + sizeof(Protocol::MouseMoveEvent))) {
                        ctx.injector.InjectMouseMove(*(Protocol::MouseMoveEvent*)(buf + sizeof(Protocol::InputHeader)));
                    }
                } else if (type == Protocol::PacketType::Feedback && len >= (int)sizeof(Protocol::FeedbackHeader)) {
                    Protocol::FeedbackHeader* fb = (Protocol::FeedbackHeader*)buf;
                    if (fb->lossRate > 0.05f) {
                        ctx.currentBitrate = std::max(1000, (int)ctx.currentBitrate - 500);
                    } else if (fb->lossRate < 0.01f) {
                        ctx.currentBitrate = std::min(10000, (int)ctx.currentBitrate + 200);
                    }
                }
            }
        }
    });

    std::cout << "Host running on " << ip << ". Press Enter to stop." << std::endl;
    std::cin.get();

    ctx.running = false;
    ctx.captureQueue.stop();
    ctx.encodeQueue.stop();
    ctx.sendQueue.stop();

    if (captureThread.joinable()) captureThread.join();
    if (encoderThread.joinable()) encoderThread.join();
    if (packetizerThread.joinable()) packetizerThread.join();
    if (senderThread.joinable()) senderThread.join();
    if (receiverThread.joinable()) receiverThread.join();
}

void RunClient(const std::string& localIp, const std::string& hostIp) {
    std::atomic<bool> running{true};
    Network::NetworkManager net;
    if (!net.Bind(localIp, 0)) return;

    Client::Receiver receiver;
    Client::JitterBuffer jitterBuffer(3);
    Client::DecoderHW decoder;
    Client::RendererD3D11 renderer;
    HWND hwnd = GetConsoleWindow();

    if (!renderer.Initialize(hwnd, 1920, 1080)) return;
    if (!decoder.Initialize(renderer.GetDevice())) return;

    std::atomic<uint32_t> lastFrameId{0};

    std::thread netThread([&]() {
        uint8_t buf[65535];
        std::string senderIp;
        uint16_t senderPort;
        while (running) {
            int len = net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            if (len > 0) {
                Protocol::PacketType type = (Protocol::PacketType)buf[0];
                if (type == Protocol::PacketType::Video && len >= (int)sizeof(Protocol::VideoHeader)) {
                    Protocol::VideoHeader* vh = (Protocol::VideoHeader*)buf;
                    receiver.ProcessPacket(*vh, buf + sizeof(Protocol::VideoHeader));
                    lastFrameId = std::max((uint32_t)lastFrameId, vh->frameId);
                } else if (type == Protocol::PacketType::FEC && len >= (int)sizeof(Protocol::FECHeader)) {
                    Protocol::FECHeader* fh = (Protocol::FECHeader*)buf;
                    receiver.ProcessFEC(*fh, buf + sizeof(Protocol::FECHeader));
                }
            }
        }
    });

    std::thread feedbackThread([&]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            Protocol::FeedbackHeader fb;
            fb.type = (uint8_t)Protocol::PacketType::Feedback;
            fb.lastReceivedFrameId = lastFrameId;
            fb.lossRate = 0.0f;
            fb.rttMs = 10;
            net.SendTo(&fb, sizeof(fb), hostIp, 5005);
        }
    });

    Client::InputCapture inputCap([&](const std::vector<uint8_t>& data) {
        net.SendTo(data.data(), data.size(), hostIp, 5005);
    });
    std::thread inputThread([&]() {
        while (running) {
            inputCap.PollGamepads();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    uint8_t handshake = (uint8_t)Protocol::PacketType::Handshake;
    net.SendTo(&handshake, 1, hostIp, 5005);

    while (running) {
        while (auto frame = receiver.GetNextFrame()) {
            jitterBuffer.PushFrame(std::move(frame));
        }

        auto frame = jitterBuffer.PopFrame();
        if (frame) {
            void* texture = nullptr;
            if (decoder.DecodeFrame(frame->buffer.data(), (uint32_t)frame->totalSize, &texture)) {
                renderer.Render((ID3D11Texture2D*)texture);
            }
            receiver.ReturnToPool(std::move(frame));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    running = false;
    if (netThread.joinable()) netThread.join();
    if (feedbackThread.joinable()) feedbackThread.join();
    if (inputThread.joinable()) inputThread.join();
}
#else
void RunHost(const std::string& ip) {}
void RunClient(const std::string& localIp, const std::string& hostIp) {}
#endif

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    if (args.size() < 2) { ShowUsage(); return 1; }

    auto interfaces = Network::NetworkManager::EnumerateInterfaces();
    std::string selectedIp = "127.0.0.1";

    if (args[1] == "--list") {
        for (size_t i = 0; i < interfaces.size(); ++i) {
            std::cout << "[" << i << "] " << interfaces[i].name << " (" << interfaces[i].ip << ")" << std::endl;
        }
        return 0;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--iface" && i + 1 < args.size()) {
            int idx = std::stoi(args[i + 1]);
            if (idx >= 0 && idx < (int)interfaces.size()) selectedIp = interfaces[idx].ip;
        }
    }

    if (args[1] == "--host") {
        RunHost(selectedIp);
    } else if (args[1] == "--client" && args.size() > 2) {
        RunClient(selectedIp, args[2]);
    } else {
        ShowUsage();
    }

    return 0;
}
