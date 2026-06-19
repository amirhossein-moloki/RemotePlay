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
#include <condition_variable>
#include <set>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include "host/capture_dxgi.hpp"
#include "host/encoder_manager.hpp"
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
    std::lock_guard<std::mutex> lock(m_pendingClientsMutex);
    m_pendingClients.clear();
}

void SessionManager::reportError(ParsecError error, const std::string& message) {
    LOG_ERROR("Session", "Error Reported: " + message);
    if (m_errorCallback) {
        m_errorCallback(error, message.c_str());
    }
}

void SessionManager::approveConnection(const std::string& ip, uint16_t port, bool approved) {
    std::lock_guard<std::mutex> lock(m_pendingClientsMutex);
    for (auto& client : m_pendingClients) {
        if (client.ip == ip && client.port == port) {
            client.approved = approved;
            client.waiting = false;
            break;
        }
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
        Host::EncoderManager encoder;
        Host::InputInjector injector;
        std::mutex clientsMutex;
        std::set<std::pair<std::string, uint16_t>> clients;
        LockFreeQueue<CapturedFrame, 2> captureQueue;
        LockFreeQueue<std::vector<Host::EncodedPacket>, 2> encodeQueue;
        LockFreeQueue<OutgoingPacket, 4096> sendQueue;
        PacketPool packetPool{200, 1024 * 1024};
        PacketPool udpPool{1000, 1500};
        std::atomic<uint32_t> globalPacketSequence{0};

        std::mutex initMutex;
        std::condition_variable initCv;
        bool initDone = false;
        bool initFailed = false;
        int capturedWidth = 1920;
        int capturedHeight = 1080;
    } ctx;

    if (!ctx.net.Bind(config.selectedIp, 5005)) {
        reportError(ParsecError::NETWORK_BIND_FAILED, "Failed to bind to " + std::string(config.selectedIp) + ":5005. Is the port already in use?");
        m_running = false;
        return;
    }

    std::thread captureThread;

    captureThread = std::thread([&]() {
        // Initialize capture on this thread for DXGI thread affinity
        if (!ctx.capture.Initialize()) {
            std::lock_guard<std::mutex> lock(ctx.initMutex);
            ctx.initFailed = true;
            ctx.initCv.notify_all();
            return;
        }

        // Detect resolution
        ID3D11Texture2D* testTex = nullptr;
        HRESULT hr = ctx.capture.AcquireFrame(&testTex);
        if (SUCCEEDED(hr)) {
            D3D11_TEXTURE2D_DESC desc;
            testTex->GetDesc(&desc);
            {
                std::lock_guard<std::mutex> lock(ctx.initMutex);
                ctx.capturedWidth = desc.Width;
                ctx.capturedHeight = desc.Height;

                // Apply resolution scale if requested
                if (config.resolutionScale > 0.0f && config.resolutionScale < 1.0f) {
                    ctx.capturedWidth = (int)(ctx.capturedWidth * config.resolutionScale);
                    ctx.capturedHeight = (int)(ctx.capturedHeight * config.resolutionScale);
                    // Ensure even dimensions for video encoding
                    ctx.capturedWidth &= ~1;
                    ctx.capturedHeight &= ~1;
                }

                ctx.initDone = true;
            }
            ctx.capture.ReleaseFrame();
            ctx.initCv.notify_all();
        } else {
            std::lock_guard<std::mutex> lock(ctx.initMutex);
            ctx.initFailed = true;
            ctx.initCv.notify_all();
            return;
        }

        uint32_t frameCount = 0;
        auto lastFpsCheck = std::chrono::high_resolution_clock::now();

        uint32_t lastProcessedFrameCount = 0;
        while (m_running) {
            frameCount++;
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsCheck).count();
            if (elapsed >= 1000) {
                Profiler::getInstance().recordValue("FPS", (double)frameCount * 1000.0 / elapsed);

                // Calculate Frame Drop Rate (simplified: captured vs encoded)
                // In a real scenario, we'd track actual dropped frames in the queue
                float dropRate = 0.0f;
                uint32_t currentTotal = frameCount;
                if (currentTotal > 0) {
                    // This is a rough heuristic for the example
                    auto fpsStats = Profiler::getInstance().getStats("FPS");
                    if (fpsStats.latest < config.fps * 0.8) dropRate = 0.2f;
                }

                // Adaptive Performance Monitoring
                auto encodeStats = Profiler::getInstance().getStats("Encode_Time");
                ctx.encoder.UpdatePerformanceMetrics(dropRate, (float)encodeStats.avg / 1000.0f);

                // Update real-time bitrate telemetry based on current profile
                Profiler::getInstance().recordValue("Host_Bitrate", (double)ctx.encoder.GetCurrentBitrate() / 1000.0);

                frameCount = 0;
                lastFpsCheck = now;
            }

            ID3D11Texture2D* tex = nullptr;
            uint64_t captureTs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

            // Encode on this thread to keep DXGI affinity and reduce latency
            HRESULT hr = ctx.capture.AcquireFrame(&tex);
            if (SUCCEEDED(hr)) {
                uint64_t endCaptureTs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                Profiler::getInstance().recordTime("Capture_Time", (double)(endCaptureTs - captureTs));

                static thread_local std::vector<Host::EncodedPacket> packets;
                packets.clear();
                uint64_t encodeStart = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                if (ctx.encoder.EncodeFrame(tex, packets, ctx.packetPool)) {
                    uint64_t encodeEnd = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                    Profiler::getInstance().recordTime("Encode_Time", (double)(encodeEnd - encodeStart));
                    for (auto& p : packets) {
                        p.captureTimestamp = endCaptureTs; // Use end of capture as ref
                        p.encodeStartTimestamp = encodeStart;
                        p.encodeEndTimestamp = encodeEnd;
                    }
                    if (!ctx.encodeQueue.push(std::move(packets))) {
                        for (auto& p : packets) ctx.packetPool.release(std::move(p.packet));
                    }
                }
                ctx.capture.ReleaseFrame();
            } else if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
                // Ignore timeouts
                std::this_thread::yield();
            } else if (m_running) {
                // Fatal error or loss of access
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                ctx.capture.Initialize();
            }
        }
    });


    std::thread packetizerThread = std::thread([&]() {
        uint32_t frameId = 0;
        std::vector<Host::EncodedPacket> frames;
        while (m_running || !ctx.encodeQueue.empty()) {
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
                                    if (!ctx.sendQueue.push({std::move(sendPkt), client.first, client.second})) {
                                        ctx.udpPool.release(std::move(sendPkt));
                                    }
                                }
                            }
                        }
                    }
                    for (auto& frag : fragments) ctx.udpPool.release(std::move(frag));
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
        while (m_running || !ctx.sendQueue.empty()) {
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
                if (type == Protocol::PacketType::Handshake && len >= (int)sizeof(Protocol::HandshakePacket)) {
                    Protocol::HandshakePacket* hp = (Protocol::HandshakePacket*)buf;
                    std::string username(hp->username, strnlen(hp->username, sizeof(hp->username)));

                    bool alreadyActive = false;
                    {
                        std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                        if (ctx.clients.count({senderIp, senderPort})) alreadyActive = true;
                    }

                    if (!alreadyActive) {
                        bool approved = false;
                        bool shouldRespond = false;
                        {
                            std::lock_guard<std::mutex> pLock(m_pendingClientsMutex);
                            auto it = std::find_if(m_pendingClients.begin(), m_pendingClients.end(), [&](const PendingClient& pc) {
                                return pc.ip == senderIp && pc.port == senderPort;
                            });

                            if (it == m_pendingClients.end()) {
                                if (config.autoApprove) {
                                    approved = true;
                                    shouldRespond = true;
                                } else {
                                    if (m_pendingClients.size() < 32) {
                                        m_pendingClients.push_back({senderIp, senderPort, username, false, true});
                                        if (m_connectionCallback) {
                                            m_connectionCallback(username.c_str(), senderIp.c_str(), senderPort);
                                        }
                                    }
                                    // Don't respond yet, wait for approval
                                    continue;
                                }
                            } else if (!it->waiting) {
                                approved = it->approved;
                                shouldRespond = true;
                                if (!approved) m_pendingClients.erase(it); // Remove rejected
                            } else {
                                // Still waiting for approval
                                continue;
                            }
                        }

                        if (shouldRespond) {
                            Protocol::HandshakeResponsePacket hrp;
                            hrp.type = (uint8_t)Protocol::PacketType::HandshakeResponse;
                            hrp.approved = approved ? 1 : 0;
                            ctx.net.SendTo(&hrp, sizeof(hrp), senderIp, senderPort);

                            if (approved) {
                                std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                                ctx.clients.insert({senderIp, senderPort});
                                ctx.encoder.RequestKeyframe();
                                // Remove from pending after adding to clients
                                std::lock_guard<std::mutex> pLock(m_pendingClientsMutex);
                                auto it = std::find_if(m_pendingClients.begin(), m_pendingClients.end(), [&](const PendingClient& pc) {
                                    return pc.ip == senderIp && pc.port == senderPort;
                                });
                                if (it != m_pendingClients.end()) m_pendingClients.erase(it);
                            }
                        }
                    } else {
                        // Already active, re-send approval in case the first one was lost
                        Protocol::HandshakeResponsePacket hrp;
                        hrp.type = (uint8_t)Protocol::PacketType::HandshakeResponse;
                        hrp.approved = 1;
                        ctx.net.SendTo(&hrp, sizeof(hrp), senderIp, senderPort);
                    }
                } else if (type == Protocol::PacketType::Input && len >= (int)sizeof(Protocol::InputHeader)) {
                    Protocol::InputHeader* ih = (Protocol::InputHeader*)buf;
                    uint8_t* payload = buf + sizeof(Protocol::InputHeader);
                    int payloadLen = len - (int)sizeof(Protocol::InputHeader);

                    if (ih->inputType == (uint8_t)Protocol::InputType::Keyboard && payloadLen >= (int)sizeof(Protocol::KeyboardEvent)) {
                        ctx.injector.InjectKeyboard(*(Protocol::KeyboardEvent*)payload);
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseMove && payloadLen >= (int)sizeof(Protocol::MouseMoveEvent)) {
                        ctx.injector.InjectMouseMove(*(Protocol::MouseMoveEvent*)payload);
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseButton && payloadLen >= (int)sizeof(Protocol::MouseButtonEvent)) {
                        ctx.injector.InjectMouseButton(*(Protocol::MouseButtonEvent*)payload);
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseScroll && payloadLen >= (int)sizeof(Protocol::MouseScrollEvent)) {
                        ctx.injector.InjectMouseScroll(*(Protocol::MouseScrollEvent*)payload);
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::GamepadStatus && payloadLen >= (int)sizeof(Protocol::GamepadStatusEvent)) {
                        ctx.injector.HandleGamepadStatus(senderIp, *(Protocol::GamepadStatusEvent*)payload);
                    } else if (ih->inputType == (uint8_t)Protocol::InputType::Gamepad && payloadLen >= (int)sizeof(Protocol::GamepadState)) {
                        ctx.injector.InjectGamepad(senderIp, *(Protocol::GamepadState*)payload);
                    }
                } else if (type == Protocol::PacketType::Feedback && len >= (int)sizeof(Protocol::FeedbackHeader)) {
                    Protocol::FeedbackHeader* fh = (Protocol::FeedbackHeader*)buf;
                    // Update telemetry for local display
                    Profiler::getInstance().recordValue("Network_LossRate", fh->lossRate);
                    Profiler::getInstance().recordValue("Network_RTT", (double)fh->rttMs);

                    // Pass to encoder manager for adaptive quality
                    ctx.encoder.UpdatePerformanceMetrics(fh->lossRate, -1.0f, fh->avgDecodeTimeMs);
                }
            }
        }
    });

    // Wait for capture initialization
    {
        std::unique_lock<std::mutex> lock(ctx.initMutex);
        ctx.initCv.wait(lock, [&] { return ctx.initDone || ctx.initFailed || !m_running; });
        if (ctx.initFailed || !m_running) {
            if (ctx.initFailed) reportError(ParsecError::HARDWARE_INIT_FAILED, "Capture initialization failed. Please check your GPU drivers and ensure you are not in a RDP session.");
            m_running = false;
        }
    }

    if (!m_running) {
        if (captureThread.joinable()) captureThread.join();
        if (packetizerThread.joinable()) packetizerThread.join();
        if (senderThread.joinable()) senderThread.join();
        if (receiverThread.joinable()) receiverThread.join();
        return;
    }

    if (!ctx.encoder.Initialize(ctx.capturedWidth, ctx.capturedHeight, config.fps, ctx.capture.GetDevice())) {
        reportError(ParsecError::HARDWARE_INIT_FAILED, "Encoder initialization failed. No compatible hardware or software encoders could be started.");
        m_running = false;
    }

    if (captureThread.joinable()) captureThread.join();
    if (packetizerThread.joinable()) packetizerThread.join();
    if (senderThread.joinable()) senderThread.join();
    if (receiverThread.joinable()) receiverThread.join();
}

void SessionManager::runClient(ParsecConfig config) {
    LOG_INFO("Session", "Starting Client Session");

    Network::NetworkManager net;
    if (!net.Bind(config.selectedIp, 0)) {
        reportError(ParsecError::NETWORK_BIND_FAILED, "Failed to bind to local interface " + std::string(config.selectedIp));
        m_running = false;
        return;
    }

    Client::Receiver receiver;
    Client::JitterBuffer jitterBuffer(3);
    Client::DecoderHW decoder;
    Client::RendererD3D11 renderer;
    Client::InputCapture input([&](const uint8_t* data, size_t size) {
        net.SendTo(data, (int)size, config.hostIp, 5005);
    });

    bool useRenderer = (config.windowHandle != nullptr);
    if (useRenderer) {
        input.RegisterDevices((HWND)config.windowHandle);
        m_activeInputCapture = &input;
    }
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
    std::atomic<bool> handshakeApproved{false};
    std::atomic<bool> handshakeRejected{false};

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
                } else if (type == Protocol::PacketType::HandshakeResponse && len >= (int)sizeof(Protocol::HandshakeResponsePacket)) {
                    Protocol::HandshakeResponsePacket* hrp = (Protocol::HandshakeResponsePacket*)buf;
                    if (hrp->approved) handshakeApproved = true;
                    else handshakeRejected = true;
                }
            }
        }
    });

    Protocol::HandshakePacket hp;
    hp.type = (uint8_t)Protocol::PacketType::Handshake;
    strncpy(hp.username, config.username, sizeof(hp.username) - 1);
    hp.username[sizeof(hp.username) - 1] = '\0';

    auto startHandshake = std::chrono::steady_clock::now();
    auto lastHandshakeSend = std::chrono::steady_clock::now();
    net.SendTo(&hp, sizeof(hp), config.hostIp, 5005);

    while (m_running && !handshakeApproved && !handshakeRejected) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHandshakeSend).count() >= 1) {
             net.SendTo(&hp, sizeof(hp), config.hostIp, 5005);
             lastHandshakeSend = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (std::chrono::duration_cast<std::chrono::seconds>(now - startHandshake).count() > 30) {
            LOG_ERROR("Session", "Handshake timeout");
            break;
        }
    }

    if (!handshakeApproved) {
        LOG_ERROR("Session", handshakeRejected ? "Connection rejected by host" : "Handshake failed");
        reportError(handshakeRejected ? ParsecError::HANDSHAKE_REJECTED : ParsecError::HANDSHAKE_TIMEOUT,
                    handshakeRejected ? "The host rejected your connection request." : "Failed to connect to host: Handshake timeout. Check the IP and network connectivity.");
        m_running = false;
        if (netThread.joinable()) netThread.join();
        return;
    }

    LOG_INFO("Session", "Handshake approved, starting stream");

    auto lastFeedbackTime = std::chrono::steady_clock::now();

    while (m_running) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFeedbackTime).count() >= 1000) {
            Protocol::FeedbackHeader fh;
            fh.type = (uint8_t)Protocol::PacketType::Feedback;
            fh.lastReceivedFrameId = lastFrameId;

            auto stats = Profiler::getInstance().getStats("Network_LossRate");
            fh.lossRate = (float)stats.latest;

            stats = Profiler::getInstance().getStats("Network_RTT");
            fh.rttMs = (uint32_t)stats.latest;

            stats = Profiler::getInstance().getStats("Decode_Time");
            fh.avgDecodeTimeMs = (float)stats.avg / 1000.0f; // us to ms

            net.SendTo(&fh, sizeof(fh), config.hostIp, 5005);
            lastFeedbackTime = now;
        }

        while (auto frame = receiver.GetNextFrame()) {
            jitterBuffer.PushFrame(std::move(frame));
        }

        if (useRenderer) renderer.NewFrame();

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
                    renderer.Render((ID3D11Texture2D*)outTexture);
                }
            }
            receiver.ReturnToPool(std::move(frame));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (useRenderer) renderer.EndFrame();
    }

    m_activeInputCapture = nullptr;
    if (netThread.joinable()) netThread.join();
}

void SessionManager::handleMessage(uint32_t msg, uint64_t wParam, int64_t lParam) {
#ifdef _WIN32
    if (m_activeInputCapture) {
        if (msg == WM_INPUT) {
            ((Client::InputCapture*)m_activeInputCapture)->HandleRawInput((LPARAM)lParam);
        }
    }
#endif
}

#else
void SessionManager::runHost(ParsecConfig config) {}
void SessionManager::runClient(ParsecConfig config) {}
#endif
