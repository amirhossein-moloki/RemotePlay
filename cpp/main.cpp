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

#include "common/profiler.hpp"
#include "common/logger.hpp"
#include "common/config.hpp"
#include "common/network_manager.hpp"
#include "common/protocol.hpp"
#include "common/safe_queue.hpp"
#include "common/lock_free_queue.hpp"
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
#include "client/overlay.hpp"
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

    LockFreeQueue<ID3D11Texture2D*, 64> captureQueue;
    LockFreeQueue<std::vector<Host::EncodedPacket>, 64> encodeQueue;
    LockFreeQueue<OutgoingPacket, 4096> sendQueue;

    PacketPool packetPool{200, 1024 * 1024};
    PacketPool udpPool{1000, 1500};

    std::atomic<int> currentBitrate{5000};
    std::atomic<uint32_t> globalPacketSequence{0};
};

void RunHost(const std::string& ip) {
    auto& config = Config::getInstance();
    int bitrate = config.getInt("host.bitrate", 5000);
    int fps = config.getInt("host.fps", 60);

    HostContext ctx;
    ctx.currentBitrate = bitrate;

    // Pre-warm pools to ensure zero allocations during streaming
    for (int i = 0; i < 50; ++i) {
        auto p = ctx.packetPool.acquire();
        if (p) ctx.packetPool.release(std::move(p));
    }
    for (int i = 0; i < 2000; ++i) {
        auto p = ctx.udpPool.acquire();
        if (p) ctx.udpPool.release(std::move(p));
    }

    if (!ctx.net.Bind(ip, 5005)) {
        LOG_ERROR("Host", "Failed to bind network to " + ip);
        return;
    }
    if (!ctx.capture.Initialize()) {
        LOG_ERROR("Host", "Failed to initialize DXGI capture");
        return;
    }
    if (!ctx.encoder.Initialize(1920, 1080, fps, ctx.currentBitrate)) {
        LOG_ERROR("Host", "Failed to initialize Hardware Encoder");
        return;
    }

    LOG_INFO("Host", "Streaming started at " + std::to_string(fps) + " FPS, " + std::to_string(bitrate) + " kbps");

    uint32_t packetsReceived = 0;

    std::thread captureThread([&]() {
        while (ctx.running) {
            ID3D11Texture2D* tex = nullptr;
            ScopeTimer timer("Capture_Time");
            if (ctx.capture.AcquireFrame(&tex)) {
                if (!ctx.captureQueue.push(std::move(tex))) {
                    ctx.capture.ReleaseFrame(); // Drop if queue full
                }
            } else if (ctx.running) {
                // Try to re-init if AcquireFrame fails (it cleans up on device lost)
                ctx.capture.Initialize();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::thread encoderThread([&]() {
        while (ctx.running) {
            ID3D11Texture2D* tex = nullptr;
            if (ctx.captureQueue.pop(tex)) {
                if (!ctx.encoder.IsInitialized()) {
                    LOG_WARN("Encoder", "Encoder not initialized, attempting recovery...");
                    ctx.encoder.Initialize(1920, 1080, 60, ctx.currentBitrate);
                }

                ctx.encoder.SetBitrate(ctx.currentBitrate);

                static thread_local std::vector<Host::EncodedPacket> packets;
                packets.clear();

                ScopeTimer timer("Encode_Time");
                if (ctx.encoder.EncodeFrame(tex, packets, ctx.packetPool)) {
                    if (!ctx.encodeQueue.push(std::move(packets))) {
                        // If queue full, packets will be cleared next loop, but we need to release their buffers
                        for (auto& p : packets) ctx.packetPool.release(std::move(p.packet));
                    }
                } else {
                    LOG_ERROR("Encoder", "EncodeFrame failed, shutting down encoder for restart.");
                    ctx.encoder.Shutdown();
                }
                ctx.capture.ReleaseFrame();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    std::thread packetizerThread([&]() {
        uint32_t frameId = 0;
        std::vector<Host::EncodedPacket> frames;

        while (ctx.running) {
            if (ctx.encodeQueue.pop(frames)) {
                ScopeTimer timer("Packetizer_Total");
                for (auto& frame : frames) {
                uint16_t totalFrags = (uint16_t)((frame.packet->size + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);

                // Use a fixed-size array on stack to avoid vector allocations
                PacketPool::Packet* fragmentPtrs[1024];
                if (totalFrags > 1024) continue;

                for (uint16_t i = 0; i < totalFrags; ++i) {
                    auto udpPkt = ctx.udpPool.acquire();
                    if (!udpPkt) {
                        for (uint16_t j = 0; j < i; ++j) ctx.udpPool.release(std::unique_ptr<PacketPool::Packet>(fragmentPtrs[j]));
                        goto next_frame;
                    }
                    Protocol::VideoHeader* vh = (Protocol::VideoHeader*)udpPkt->data.data();
                    vh->type = (uint8_t)Protocol::PacketType::Video;
                    vh->frameId = frameId;
                    vh->fragmentIndex = i;
                    vh->totalFragments = totalFrags;
                    vh->packetSequence = ctx.globalPacketSequence++;
                    vh->timestamp = frame.timestamp;
                    vh->captureTimestamp = frame.timestamp; // We use the same for now, but in CaptureDXGI we could set a more precise one
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
                        // Calculate FEC before pushing the source packet to the send queue
                        // to avoid race condition with the sender thread.
                        const int FEC_GROUP_SIZE = 5;
                        size_t groupStart = (i / FEC_GROUP_SIZE) * FEC_GROUP_SIZE;
                        bool isLastInGroup = ((i + 1) % FEC_GROUP_SIZE == 0 || i == totalFrags - 1);

                        auto sendPkt = ctx.udpPool.acquire();
                        if (sendPkt) {
                            sendPkt->size = fragmentPtrs[i]->size;
                            memcpy(sendPkt->data.data(), fragmentPtrs[i]->data.data(), fragmentPtrs[i]->size);
                            ctx.sendQueue.push({std::move(sendPkt), client.first, client.second});
                        }

                        if (isLastInGroup) {
                            uint16_t count = (uint16_t)(i - groupStart + 1);
                            if (count > 1) {
                                auto fecPkt = ctx.udpPool.acquire();
                                if (fecPkt) {
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

                                        for (size_t k = 0; k < vh->dataSize; ++k) {
                                            fecPayload[k] ^= fragPayload[k];
                                        }
                                    }
                                    fecPkt->size = sizeof(Protocol::FECHeader) + fh->dataSize;
                                    ctx.sendQueue.push({std::move(fecPkt), client.first, client.second});
                                }
                            }
                        }
                    }
                }

                for (size_t i = 0; i < totalFrags; ++i) {
                    ctx.udpPool.release(std::unique_ptr<PacketPool::Packet>(fragmentPtrs[i]));
                }
            next_frame:
                ctx.packetPool.release(std::move(frame.packet));
            }
            frameId++;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
});

    std::thread senderThread([&]() {
        const size_t MAX_BATCH = 64;
        Network::NetworkManager::BatchItem batch[MAX_BATCH];
        OutgoingPacket ops[MAX_BATCH];

        while (ctx.running) {
            size_t count = 0;
            while (count < MAX_BATCH && ctx.sendQueue.pop(ops[count])) {
                batch[count].data = ops[count].packet->data.data();
                batch[count].size = ops[count].packet->size;
                batch[count].targetIp = ops[count].targetIp;
                batch[count].targetPort = ops[count].targetPort;
                count++;
            }

            if (count > 0) {
                {
                    ScopeTimer timer("Network_SendBatch");
                    ctx.net.SendBatch(batch, count);
                }
                for (size_t i = 0; i < count; ++i) {
                    ctx.udpPool.release(std::move(ops[i].packet));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    std::thread receiverThread([&]() {
        uint8_t buf[2048];
        std::string senderIp;
        uint16_t senderPort;
        while (ctx.running) {
            int len = ctx.net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            if (len > 0) {
                packetsReceived++;
                Protocol::PacketType type = (Protocol::PacketType)buf[0];
                if (type == Protocol::PacketType::Handshake) {
                    std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                    ctx.clients.insert({senderIp, senderPort});
                } else if (type == Protocol::PacketType::Input && len >= (int)sizeof(Protocol::InputHeader)) {
                    Protocol::InputHeader* ih = (Protocol::InputHeader*)buf;

                    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                    Profiler::getInstance().recordTime("Input_NetworkLatency", (double)(now - ih->timestamp));

                    if (ih->inputType == (uint8_t)Protocol::InputType::Keyboard && len >= (int)(sizeof(Protocol::InputHeader) + sizeof(Protocol::KeyboardEvent))) {
                        ScopeTimer timer("Input_Injection_Keyboard");
                        ctx.injector.InjectKeyboard(*(Protocol::KeyboardEvent*)(buf + sizeof(Protocol::InputHeader)));
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseMove && len >= (int)(sizeof(Protocol::InputHeader) + sizeof(Protocol::MouseMoveEvent))) {
                        ScopeTimer timer("Input_Injection_MouseMove");
                        ctx.injector.InjectMouseMove(*(Protocol::MouseMoveEvent*)(buf + sizeof(Protocol::InputHeader)));
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::GamepadStatus && len >= (int)(sizeof(Protocol::InputHeader) + sizeof(Protocol::GamepadStatusEvent))) {
                        ScopeTimer timer("Input_Injection_GamepadStatus");
                        ctx.injector.HandleGamepadStatus(senderIp, *(Protocol::GamepadStatusEvent*)(buf + sizeof(Protocol::InputHeader)));
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::Gamepad && len >= (int)(sizeof(Protocol::InputHeader) + sizeof(Protocol::GamepadState))) {
                        ScopeTimer timer("Input_Injection_Gamepad");
                        ctx.injector.InjectGamepad(senderIp, *(Protocol::GamepadState*)(buf + sizeof(Protocol::InputHeader)));
                    }
                } else if (type == Protocol::PacketType::Feedback && len >= (int)sizeof(Protocol::FeedbackHeader)) {
                    Protocol::FeedbackHeader* fb = (Protocol::FeedbackHeader*)buf;
                    Profiler::getInstance().recordValue("Network_LossRate", fb->lossRate);
                    Profiler::getInstance().recordValue("Network_RTT", (double)fb->rttMs);
                    Profiler::getInstance().recordValue("Host_Bitrate", (double)ctx.currentBitrate);
                    if (fb->lossRate > 0.05f) {
                        ctx.currentBitrate = std::max(1000, (int)ctx.currentBitrate - 500);
                    } else if (fb->lossRate < 0.01f) {
                        ctx.currentBitrate = std::min(10000, (int)ctx.currentBitrate + 200);
                    }
                }
            }
        }
    });

    LOG_INFO("Host", "Host running on " + ip + ". Press Enter to stop.");
    std::cin.get();

    ctx.running = false;

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

    if (!renderer.Initialize(hwnd, 1920, 1080)) {
        LOG_ERROR("Client", "Failed to initialize Renderer");
        return;
    }

    Client::InputCapture inputCap([&](const std::vector<uint8_t>& data) {
        net.SendTo(data.data(), data.size(), hostIp, 5005);
    });

    // We need a message loop for Raw Input
    // In a real app, this would be part of the main window loop
    inputCap.RegisterDevices(hwnd);

    if (!decoder.Initialize(renderer.GetDevice())) {
        LOG_ERROR("Client", "Failed to initialize Decoder");
        return;
    }

    LOG_INFO("Client", "Connecting to " + hostIp + "...");

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

                    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                    Profiler::getInstance().recordTime("EndToEnd_Latency", (double)(now - vh->captureTimestamp));

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

    std::thread inputThread([&]() {
        while (running) {
            inputCap.PollGamepads();
            std::this_thread::sleep_for(std::chrono::milliseconds(4)); // ~250Hz polling for sub-5ms latency
        }
    });

    std::thread watchdogThread([&]() {
        uint32_t lastProcessedFrame = 0;
        int timeoutCount = 0;
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (lastFrameId == lastProcessedFrame && lastFrameId != 0) {
                timeoutCount++;
                if (timeoutCount > 5) {
                    LOG_WARN("Client", "No new frames for 5 seconds, triggering re-handshake...");
                    uint8_t handshake = (uint8_t)Protocol::PacketType::Handshake;
                    net.SendTo(&handshake, 1, hostIp, 5005);
                    timeoutCount = 0;
                }
            } else {
                timeoutCount = 0;
            }
            lastProcessedFrame = lastFrameId;
        }
    });

    uint8_t handshake = (uint8_t)Protocol::PacketType::Handshake;
    net.SendTo(&handshake, 1, hostIp, 5005);

    while (running) {
        // Handle Windows messages for Raw Input
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            if (msg.message == WM_INPUT) inputCap.HandleRawInput(msg.lParam);
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        renderer.NewFrame();
        Client::Overlay::Render();

        while (auto frame = receiver.GetNextFrame()) {
            jitterBuffer.PushFrame(std::move(frame));
        }

        auto frame = jitterBuffer.PopFrame();
        if (frame) {
            void* texture = nullptr;
            {
                ScopeTimer timer("Decode_Time");
                if (decoder.DecodeFrame(frame->buffer.data(), (uint32_t)frame->totalSize, &texture)) {
                    ScopeTimer pTimer("Present_Time");
                    renderer.Render((ID3D11Texture2D*)texture);
                }
            }
            receiver.ReturnToPool(std::move(frame));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        renderer.EndFrame();
    }

    running = false;
    if (netThread.joinable()) netThread.join();
    if (feedbackThread.joinable()) feedbackThread.join();
    if (inputThread.joinable()) inputThread.join();
    if (watchdogThread.joinable()) watchdogThread.join();
}
#else
void RunHost(const std::string& ip) {}
void RunClient(const std::string& localIp, const std::string& hostIp) {}
#endif

int main(int argc, char* argv[]) {
    Logger::getInstance().init("parsec-lite.log");
    Config::getInstance().load("config.ini");

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
