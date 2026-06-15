#include "session_manager.hpp"
#include "logger.hpp"
#include "config.hpp"
#include "profiler.hpp"
#include "network_manager.hpp"
#include "protocol.hpp"
#include "safe_queue.hpp"
#include "lock_free_queue.hpp"
#include "packet_pool.hpp"
#include "fixed_ring_buffer.hpp"

#include <vector>
#include <mutex>
#include <set>
#include <algorithm>
#include <cstring>

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

#include <chrono>

struct OutgoingPacket {
    std::unique_ptr<PacketPool::Packet> packet;
    std::string targetIp;
    uint16_t targetPort;
};

#ifdef _WIN32
struct CapturedFrame {
    ID3D11Texture2D* texture;
    uint64_t captureTimestamp;
};
#endif

void SessionManager::startSession(ParsecConfig config) {
    if (m_running) stopSession();

    m_running = true;
    m_currentConfig = config;
    m_sessionThread = std::thread([this, config]() {
        if (config.isHost) {
            runHost(config);
        } else {
            runClient(config);
        }
    });
}

void SessionManager::stopSession() {
    m_running = false;
    if (m_sessionThread.joinable()) {
        m_sessionThread.join();
    }
}

bool SessionManager::getTelemetry(ParsecTelemetry* outTelemetry) {
    if (!outTelemetry) return false;

    auto& profiler = Profiler::getInstance();
    outTelemetry->e2eLatency = (float)profiler.getStats("EndToEnd_Latency").p99 / 1000.0f; // us to ms
    outTelemetry->captureTime = (float)profiler.getStats("Capture_Time").avg / 1000.0f; // us to ms
    outTelemetry->encodeTime = (float)profiler.getStats("Encode_Time").avg / 1000.0f; // us to ms
    outTelemetry->networkTime = (float)profiler.getStats("Network_Latency").avg / 1000.0f; // us to ms
    outTelemetry->decodeTime = (float)profiler.getStats("Decode_Time").avg / 1000.0f; // us to ms
    outTelemetry->fps = (float)profiler.getStats("FPS").latest;
    outTelemetry->lossRate = (float)profiler.getStats("Network_LossRate").latest;
    outTelemetry->rtt = (float)profiler.getStats("Network_RTT").latest;
    outTelemetry->bitrateMbps = (float)profiler.getStats("Host_Bitrate").latest;

    return true;
}

#ifdef _WIN32
void SessionManager::runHost(ParsecConfig config) {
    LOG_INFO("Session", "Starting Host Session");

    struct HostContext {
        Network::NetworkManager net;
        Host::CaptureDXGI capture;
        Host::FFmpegHardwareEncoder encoder;
        Host::InputInjector injector;
        std::mutex clientsMutex;
        std::set<std::pair<std::string, uint16_t>> clients;
        LockFreeQueue<CapturedFrame, 2> captureQueue;
        LockFreeQueue<std::vector<Host::EncodedPacket>, 2> encodeQueue;
        LockFreeQueue<OutgoingPacket, 4096> sendQueue;
        PacketPool packetPool{200, 1024 * 1024};
        PacketPool udpPool{1000, 1500};
        std::atomic<uint32_t> globalPacketSequence{0};
    } ctx;

    if (!ctx.net.Bind(config.selectedIp, 5005)) return;
    if (!ctx.capture.Initialize()) return;

    // Resolve resolution from capture
    int width = 1920, height = 1080;
    ID3D11Texture2D* testTex = nullptr;
    if (ctx.capture.AcquireFrame(&testTex)) {
        D3D11_TEXTURE2D_DESC desc;
        testTex->GetDesc(&desc);
        width = desc.Width;
        height = desc.Height;
        ctx.capture.ReleaseFrame();
    }

    if (!ctx.encoder.Initialize(width, height, config.fps, config.bitrate, ctx.capture.GetDevice())) return;

    std::thread captureThread = std::thread([&]() {
        uint32_t frameCount = 0;
        auto lastFpsCheck = std::chrono::high_resolution_clock::now();

        while (m_running) {
            frameCount++;
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsCheck).count();
            if (elapsed >= 1000) {
                Profiler::getInstance().recordValue("FPS", (double)frameCount * 1000.0 / elapsed);
                frameCount = 0;
                lastFpsCheck = now;
            }

            ID3D11Texture2D* tex = nullptr;
            uint64_t captureTs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            if (ctx.capture.AcquireFrame(&tex)) {
                uint64_t endCaptureTs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                Profiler::getInstance().recordTime("Capture_Time", (double)(endCaptureTs - captureTs));
                if (!ctx.captureQueue.push({tex, endCaptureTs})) ctx.capture.ReleaseFrame();
            } else if (m_running) {
                ctx.capture.Initialize();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });

    std::thread encoderThread = std::thread([&]() {
        Profiler::getInstance().recordValue("Host_Bitrate", (double)config.bitrate / 1000.0);
        while (m_running) {
            CapturedFrame cf = {nullptr, 0};
            if (ctx.captureQueue.pop(cf)) {
                static thread_local std::vector<Host::EncodedPacket> packets;
                packets.clear();
                uint64_t encodeStart = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                if (ctx.encoder.EncodeFrame(cf.texture, packets, ctx.packetPool)) {
                    uint64_t encodeEnd = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                    Profiler::getInstance().recordTime("Encode_Time", (double)(encodeEnd - encodeStart));
                    for (auto& p : packets) {
                        p.captureTimestamp = cf.captureTimestamp;
                        p.encodeStartTimestamp = encodeStart;
                        p.encodeEndTimestamp = encodeEnd;
                    }
                    if (!ctx.encodeQueue.push(std::move(packets))) {
                        for (auto& p : packets) ctx.packetPool.release(std::move(p.packet));
                    }
                }
                ctx.capture.ReleaseFrame();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    std::thread packetizerThread = std::thread([&]() {
        uint32_t frameId = 0;
        std::vector<Host::EncodedPacket> frames;
        while (m_running) {
            if (ctx.encodeQueue.pop(frames)) {
                for (auto& frame : frames) {
                    uint16_t totalFrags = (uint16_t)((frame.packet->size + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);
                    std::vector<std::unique_ptr<PacketPool::Packet>> fragments;
                    for (uint16_t i = 0; i < totalFrags; ++i) {
                        auto udpPkt = ctx.udpPool.acquire();
                        if (!udpPkt) break;
                        Protocol::VideoHeader* vh = (Protocol::VideoHeader*)udpPkt->data.data();
                        vh->type = (uint8_t)Protocol::PacketType::Video;
                        vh->frameId = frameId;
                        vh->fragmentIndex = i;
                        vh->totalFragments = totalFrags;
                        vh->packetSequence = ctx.globalPacketSequence++;
                        vh->captureTimestamp = frame.captureTimestamp;
                        vh->encodeStartTimestamp = frame.encodeStartTimestamp;
                        vh->encodeEndTimestamp = frame.encodeEndTimestamp;
                        vh->flags = frame.isKeyframe ? 0x01 : 0x00;
                        vh->dataSize = (uint16_t)std::min((uint32_t)Protocol::MAX_UDP_PAYLOAD, (uint32_t)(frame.packet->size - i * Protocol::MAX_UDP_PAYLOAD));
                        memcpy(udpPkt->data.data() + sizeof(Protocol::VideoHeader), frame.packet->data.data() + (i * Protocol::MAX_UDP_PAYLOAD), vh->dataSize);
                        udpPkt->size = sizeof(Protocol::VideoHeader) + vh->dataSize;
                        fragments.push_back(std::move(udpPkt));
                    }

                    {
                        std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                        for (const auto& client : ctx.clients) {
                            for (auto& frag : fragments) {
                                auto sendPkt = ctx.udpPool.acquire();
                                if (sendPkt) {
                                    sendPkt->size = frag->size;
                                    memcpy(sendPkt->data.data(), frag->data.data(), frag->size);
                                    ctx.sendQueue.push({std::move(sendPkt), client.first, client.second});
                                }
                            }
                        }
                    }
                    ctx.packetPool.release(std::move(frame.packet));
                }
                frameId++;
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    std::thread senderThread = std::thread([&]() {
        const size_t MAX_BATCH = 64;
        Network::NetworkManager::BatchItem batch[MAX_BATCH];
        OutgoingPacket ops[MAX_BATCH];
        while (m_running) {
            size_t count = 0;
            while (count < MAX_BATCH && ctx.sendQueue.pop(ops[count])) {
                batch[count].data = ops[count].packet->data.data();
                batch[count].size = ops[count].packet->size;
                batch[count].targetIp = ops[count].targetIp;
                batch[count].targetPort = ops[count].targetPort;
                count++;
            }
            if (count > 0) {
                ctx.net.SendBatch(batch, count);
                for (size_t i = 0; i < count; ++i) ctx.udpPool.release(std::move(ops[i].packet));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    std::thread receiverThread = std::thread([&]() {
        uint8_t buf[2048];
        std::string senderIp;
        uint16_t senderPort;
        while (m_running) {
            int len = ctx.net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            if (len > 0) {
                Protocol::PacketType type = (Protocol::PacketType)buf[0];
                if (type == Protocol::PacketType::Handshake) {
                    std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                    ctx.clients.insert({senderIp, senderPort});
                } else if (type == Protocol::PacketType::Input && len >= (int)sizeof(Protocol::InputHeader)) {
                    Protocol::InputHeader* ih = (Protocol::InputHeader*)buf;
                    if (ih->inputType == (uint8_t)Protocol::InputType::Keyboard) {
                        ctx.injector.InjectKeyboard(*(Protocol::KeyboardEvent*)(buf + sizeof(Protocol::InputHeader)));
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseMove) {
                        ctx.injector.InjectMouseMove(*(Protocol::MouseMoveEvent*)(buf + sizeof(Protocol::InputHeader)));
                    }
                }
            }
        }
    });

    if (captureThread.joinable()) captureThread.join();
    if (encoderThread.joinable()) encoderThread.join();
    if (packetizerThread.joinable()) packetizerThread.join();
    if (senderThread.joinable()) senderThread.join();
    if (receiverThread.joinable()) receiverThread.join();
}

void SessionManager::runClient(ParsecConfig config) {
    LOG_INFO("Session", "Starting Client Session");

    Network::NetworkManager net;
    if (!net.Bind(config.selectedIp, 0)) return;

    Client::Receiver receiver;
    Client::JitterBuffer jitterBuffer(3);
    Client::DecoderHW decoder;
    Client::RendererD3D11 renderer;

    bool useRenderer = (config.windowHandle != nullptr);
    if (useRenderer) {
        if (!renderer.Initialize((HWND)config.windowHandle, 1920, 1080)) {
            LOG_ERROR("Session", "Failed to initialize renderer");
            useRenderer = false;
        } else if (!decoder.Initialize(renderer.GetDevice())) {
            LOG_ERROR("Session", "Failed to initialize decoder");
            useRenderer = false;
        }
    } else {
        decoder.Initialize(nullptr);
    }

    std::atomic<uint32_t> lastFrameId{0};
    std::thread netThread = std::thread([&]() {
        uint8_t buf[65535];
        std::string senderIp;
        uint16_t senderPort;
        while (m_running) {
            int len = net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            if (len > 0) {
                Protocol::PacketType type = (Protocol::PacketType)buf[0];
                if (type == Protocol::PacketType::Video && len >= (int)sizeof(Protocol::VideoHeader)) {
                    Protocol::VideoHeader* vh = (Protocol::VideoHeader*)buf;
                    receiver.ProcessPacket(*vh, buf + sizeof(Protocol::VideoHeader));
                    lastFrameId = std::max((uint32_t)lastFrameId, vh->frameId);
                }
            }
        }
    });

    uint8_t handshake = (uint8_t)Protocol::PacketType::Handshake;
    net.SendTo(&handshake, 1, config.hostIp, 5005);

    while (m_running) {
        while (auto frame = receiver.GetNextFrame()) {
            jitterBuffer.PushFrame(std::move(frame));
        }

        auto frame = jitterBuffer.PopFrame();
        if (frame) {
            uint64_t decodeStart = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            void* outTexture = nullptr;
            if (decoder.DecodeFrame(frame->buffer.data(), frame->totalSize, &outTexture)) {
                uint64_t decodeEnd = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                Profiler::getInstance().recordTime("Decode_Time", (double)(decodeEnd - decodeStart));

                uint64_t now = decodeEnd;
                Profiler::getInstance().recordTime("EndToEnd_Latency", (double)(now - frame->captureTimestamp));

                if (useRenderer && outTexture) {
                    renderer.NewFrame();
                    renderer.Render((ID3D11Texture2D*)outTexture);
                    renderer.EndFrame();
                }
            }
            receiver.ReturnToPool(std::move(frame));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    if (netThread.joinable()) netThread.join();
}
#else
void SessionManager::runHost(ParsecConfig config) {}
void SessionManager::runClient(ParsecConfig config) {}
#endif
