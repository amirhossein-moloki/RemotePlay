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
#include <cstdint>
#include <map>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include "host/capture_dxgi.hpp"
#include "host/encoder_manager.hpp"
#include "host/input_injector.hpp"
#include "host/audio_capture_wasapi.hpp"
#include "host/audio_encoder_ffmpeg.hpp"
#include "client/receiver.hpp"
#include "client/decoder_hw.hpp"
#include "client/input_capture.hpp"
#include "client/renderer_d3d11.hpp"
#include "client/jitter_buffer.hpp"
#include "client/audio_decoder_ffmpeg.hpp"
#include "client/audio_renderer_wasapi.hpp"
#include "client/audio_jitter_buffer.hpp"
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

    if (config.runBoth) {
        m_hostThread = std::thread([this, config]() { runHost(config); });
        m_clientThread = std::thread([this, config]() { runClient(config); });
    } else {
        if (config.isHost) {
            m_hostThread = std::thread([this, config]() { runHost(config); });
        } else {
            m_clientThread = std::thread([this, config]() { runClient(config); });
        }
    }
}

void SessionManager::stopSession() {
    m_running = false;
    if (m_hostThread.joinable()) {
        m_hostThread.join();
    }
    if (m_clientThread.joinable()) {
        m_clientThread.join();
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
    // Deprecated: All connections are now auto-approved
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

    struct ClientState {
        std::string ip;
        uint16_t port;
        std::unique_ptr<Host::EncoderManager> encoder;
        std::unique_ptr<Host::FFmpegAudioEncoder> audioEncoder;
        LockFreeQueue<std::vector<Host::EncodedPacket>, 4> encodeQueue;
        uint32_t frameId = 0;
        uint32_t audioSequence = 0;
    };

    struct HostContext {
        Network::NetworkManager net;
        Host::CaptureDXGI capture;
        Host::AudioCaptureWASAPI audioCapture;
        Host::InputInjector injector;
        std::mutex clientsMutex;
        std::vector<std::shared_ptr<ClientState>> clients;
        LockFreeQueue<CapturedFrame, 2> captureQueue;
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
    std::thread audioThread;

    if (config.enableAudio) {
        if (!ctx.audioCapture.Initialize()) {
            reportError(ParsecError::HARDWARE_INIT_FAILED, "Failed to initialize audio capture. System audio streaming will be disabled.");
        }
        ctx.audioCapture.Start([&](const Audio::AudioFrame& frame) {
            std::lock_guard<std::mutex> lock(ctx.clientsMutex);
            for (auto& client : ctx.clients) {
                if (client->audioEncoder) {
                    std::vector<uint8_t> encoded;
                    if (client->audioEncoder->Encode(frame, encoded)) {
                        auto udpPkt = ctx.udpPool.acquire();
                        if (udpPkt) {
                            Protocol::AudioHeader* ah = (Protocol::AudioHeader*)udpPkt->data.data();
                            ah->type = (uint8_t)Protocol::PacketType::Audio;
                            ah->sequence = client->audioSequence++;
                            ah->captureTimestamp = frame.timestamp;
                            ah->dataSize = (uint16_t)encoded.size();
                            memcpy(udpPkt->data.data() + sizeof(Protocol::AudioHeader), encoded.data(), encoded.size());
                            udpPkt->size = sizeof(Protocol::AudioHeader) + encoded.size();

                            if (!ctx.sendQueue.push({std::move(udpPkt), client->ip, client->port})) {
                                ctx.udpPool.release(std::move(udpPkt));
                            }
                        }
                    }
                }
            }
        });
    }

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

                // Per-client adaptive performance monitoring is now handled via feedback packets
                // For local telemetry, we can show an average or first client
                {
                    std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                    if (!ctx.clients.empty()) {
                        Profiler::getInstance().recordValue("Host_Bitrate", (double)ctx.clients[0]->encoder->GetCurrentBitrate() / 1000.0);
                    }
                }

                frameCount = 0;
                lastFpsCheck = now;
            }

            ID3D11Texture2D* tex = nullptr;
            uint64_t captureTs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

            // Encode independently for each client to keep DXGI affinity and support per-client profiles
            HRESULT hr = ctx.capture.AcquireFrame(&tex);
            if (SUCCEEDED(hr)) {
                uint64_t endCaptureTs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                Profiler::getInstance().recordTime("Capture_Time", (double)(endCaptureTs - captureTs));

                D3D11_TEXTURE2D_DESC capturedDesc;
                tex->GetDesc(&capturedDesc);
                LOG_INFO("StreamTrace", "CAPTURE_OK frameAttempt=" + std::to_string(frameCount) +
                         " texture=" + std::to_string(capturedDesc.Width) + "x" + std::to_string(capturedDesc.Height) +
                         " captureUs=" + std::to_string(endCaptureTs - captureTs));

                {
                    std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                    for (auto& client : ctx.clients) {
                        static thread_local std::vector<Host::EncodedPacket> packets;
                        packets.clear();
                        uint64_t encodeStart = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                        LOG_INFO("StreamTrace", "ENCODE_IN frameId=" + std::to_string(client->frameId) +
                                 " client=" + client->ip + ":" + std::to_string(client->port));
                        if (client->encoder->EncodeFrame(tex, packets, ctx.packetPool)) {
                            uint64_t encodeEnd = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                            Profiler::getInstance().recordTime("Encode_Time", (double)(encodeEnd - encodeStart));
                            size_t encodedBytes = 0;
                            for (const auto& p : packets) {
                                if (p.packet) encodedBytes += p.packet->size;
                            }
                            LOG_INFO("StreamTrace", "ENCODE_OUT frameId=" + std::to_string(client->frameId) +
                                     " packetCount=" + std::to_string(packets.size()) +
                                     " encodedBytes=" + std::to_string(encodedBytes) +
                                     " encodeUs=" + std::to_string(encodeEnd - encodeStart));
                            for (auto& p : packets) {
                                p.captureTimestamp = endCaptureTs;
                                p.encodeStartTimestamp = encodeStart;
                                p.encodeEndTimestamp = encodeEnd;
                            }
                            if (!client->encodeQueue.push(std::move(packets))) {
                                LOG_ERROR("StreamTrace", "ENCODE_QUEUE_DROP frameId=" + std::to_string(client->frameId));
                                for (auto& p : packets) ctx.packetPool.release(std::move(p.packet));
                            } else {
                                LOG_INFO("StreamTrace", "ENCODE_QUEUE_PUSH frameId=" + std::to_string(client->frameId));
                            }
                        } else {
                            LOG_ERROR("StreamTrace", "ENCODE_FAIL frameId=" + std::to_string(client->frameId));
                        }
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
        while (m_running) {
            bool worked = false;
            std::vector<std::shared_ptr<ClientState>> activeClients;
            {
                std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                activeClients = ctx.clients;
            }

            for (auto& client : activeClients) {
                std::vector<Host::EncodedPacket> frames;
                if (client->encodeQueue.pop(frames)) {
                    worked = true;
                    for (auto& frame : frames) {
                        uint16_t totalFrags = (uint16_t)((frame.packet->size + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);
                        for (uint16_t i = 0; i < totalFrags; ++i) {
                            auto udpPkt = ctx.udpPool.acquire();
                            if (!udpPkt) break;
                            Protocol::VideoHeader* vh = (Protocol::VideoHeader*)udpPkt->data.data();
                            vh->type = (uint8_t)Protocol::PacketType::Video;
                            vh->frameId = client->frameId;
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

                            if (!ctx.sendQueue.push({std::move(udpPkt), client->ip, client->port})) {
                                LOG_ERROR("StreamTrace", "SEND_QUEUE_DROP frameId=" + std::to_string(client->frameId) +
                                          " fragment=" + std::to_string(i) + "/" + std::to_string(totalFrags));
                                ctx.udpPool.release(std::move(udpPkt));
                            } else {
                                LOG_INFO("StreamTrace", "SEND_QUEUE_PUSH frameId=" + std::to_string(client->frameId) +
                                         " fragment=" + std::to_string(i) + "/" + std::to_string(totalFrags) +
                                         " bytes=" + std::to_string(vh->dataSize));
                            }
                        }
                        ctx.packetPool.release(std::move(frame.packet));
                    }
                    client->frameId++;
                }
            }

            if (!worked) {
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
                int sendResult = ctx.net.SendBatch(batch, count);
                LOG_INFO("StreamTrace", "UDP_SEND_BATCH requested=" + std::to_string(count) +
                         " result=" + std::to_string(sendResult));
                if (sendResult < 0) {
                    LOG_ERROR("StreamTrace", "UDP_SEND_FAIL requested=" + std::to_string(count));
                }
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
                    LOG_INFO("Session", "Received Handshake from " + senderIp + " (Username: " + username + ")");

                    bool alreadyActive = false;
                    {
                        std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                        for(auto& c : ctx.clients) if(c->ip == senderIp && c->port == senderPort) { alreadyActive = true; break; }
                    }

                    if (!alreadyActive) {
                        // AUTO-APPROVE: We now always approve and initialize immediately
                        auto newState = std::make_shared<ClientState>();
                        newState->ip = senderIp;
                        newState->port = senderPort;
                        newState->encoder = std::make_unique<Host::EncoderManager>();

                        if (config.enableAudio) {
                            newState->audioEncoder = std::make_unique<Host::FFmpegAudioEncoder>();
                            Audio::AudioFormat format; // We'll need to get actual format from capture or use 48k/Stereo/S16
                            format.sampleRate = 48000;
                            format.channels = 2;
                            format.bitsPerSample = 16;
                            if (!newState->audioEncoder->Initialize(format, config.audioBitrate)) {
                                LOG_ERROR("Session", "Failed to initialize audio encoder for " + senderIp);
                            }
                        }

                        if (newState->encoder->Initialize(ctx.capturedWidth, ctx.capturedHeight, config.fps, ctx.capture.GetDevice(), config.useHardwareEncoding)) {
                            {
                                std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                                ctx.clients.push_back(newState);
                            }
                            newState->encoder->RequestKeyframe();
                            LOG_INFO("Session", "Auto-approved and initialized per-client encoder for " + senderIp);

                            Protocol::HandshakeResponsePacket hrp;
                            hrp.type = (uint8_t)Protocol::PacketType::HandshakeResponse;
                            hrp.approved = 1;
                            ctx.net.SendTo(&hrp, sizeof(hrp), senderIp, senderPort);
                        } else {
                            LOG_ERROR("Session", "Failed to initialize per-client encoder for " + senderIp + ". Rejecting connection.");
                            Protocol::HandshakeResponsePacket hrp;
                            hrp.type = (uint8_t)Protocol::PacketType::HandshakeResponse;
                            hrp.approved = 0;
                            ctx.net.SendTo(&hrp, sizeof(hrp), senderIp, senderPort);
                        }
                    } else {
                        // Already active, re-send approval in case the first one was lost
                        Protocol::HandshakeResponsePacket hrp;
                        hrp.type = (uint8_t)Protocol::PacketType::HandshakeResponse;
                        hrp.approved = 1;
                        ctx.net.SendTo(&hrp, sizeof(hrp), senderIp, senderPort);
                    }
                } else if (type == Protocol::PacketType::Input && len >= (int)sizeof(Protocol::InputHeader)) {
                    bool isLoopback = (senderIp == "127.0.0.1" || senderIp == "::1");
                    if (!isLoopback) {
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
                    }
                } else if (type == Protocol::PacketType::Feedback && len >= (int)sizeof(Protocol::FeedbackHeader)) {
                    Protocol::FeedbackHeader* fh = (Protocol::FeedbackHeader*)buf;
                    // Update telemetry for local display
                    Profiler::getInstance().recordValue("Network_LossRate", fh->lossRate);
                    Profiler::getInstance().recordValue("Network_RTT", (double)fh->rttMs);

                    // Pass to specific client's encoder manager for independent ABR
                    {
                        std::lock_guard<std::mutex> lock(ctx.clientsMutex);
                        for(auto& c : ctx.clients) {
                            if(c->ip == senderIp && c->port == senderPort) {
                                c->encoder->UpdatePerformanceMetrics(fh->lossRate, -1.0f, fh->avgDecodeTimeMs);
                                if (fh->requestKeyframe) {
                                    LOG_INFO("Session", "Keyframe requested by client " + senderIp);
                                    c->encoder->RequestKeyframe();
                                }
                                break;
                            }
                        }
                    }
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

    // Encoder initialization is now per-client during handshake

    ctx.audioCapture.Stop();
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

    Client::AudioJitterBuffer audioJitterBuffer(50);
    Client::FFmpegAudioDecoder audioDecoder;
    Client::AudioRendererWASAPI audioRenderer;

    std::string hostIpStr(config.hostIp);
    bool isLoopback = (hostIpStr == "127.0.0.1" || hostIpStr == "localhost" || hostIpStr == "::1");

    Client::InputCapture input([&, isLoopback, config](const uint8_t* data, size_t size) {
        if (!isLoopback) {
            net.SendTo(data, (int)size, config.hostIp, 5005);
        }
    });

    bool useRenderer = (config.windowHandle != nullptr);
    if (useRenderer) {
        input.RegisterDevices((HWND)config.windowHandle);
        m_activeInputCapture = &input;
    }
    if (useRenderer) {
        if (!renderer.Initialize((HWND)config.windowHandle, 1920, 1080)) {
            LOG_ERROR("Session", "Failed to initialize renderer. Stopping session.");
            reportError(ParsecError::HARDWARE_INIT_FAILED, "Renderer initialization failed. Please check your GPU drivers and system compatibility.");
            m_running = false;
            return;
        } else if (!decoder.Initialize(renderer.GetDevice(), config.useHardwareEncoding)) {
            LOG_ERROR("Session", "Failed to initialize decoder. Stopping session.");
            reportError(ParsecError::HARDWARE_INIT_FAILED, "Hardware decoder initialization failed. Please ensure your GPU supports H.264 decoding.");
            m_running = false;
            return;
        }
    } else {
        decoder.Initialize(nullptr, config.useHardwareEncoding);
    }

    if (config.enableAudio) {
        Audio::AudioFormat audioFormat;
        audioFormat.sampleRate = 48000;
        audioFormat.channels = 2;
        audioFormat.bitsPerSample = 16;
        if (!audioDecoder.Initialize(audioFormat)) {
            LOG_ERROR("Session", "Failed to initialize audio decoder");
            reportError(ParsecError::HARDWARE_INIT_FAILED, "Audio decoder failed to initialize.");
        }
        if (!audioRenderer.Initialize(audioFormat)) {
            LOG_ERROR("Session", "Failed to initialize audio renderer");
            reportError(ParsecError::HARDWARE_INIT_FAILED, "Audio renderer failed to initialize.");
        }
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
                    LOG_INFO("StreamTrace", "UDP_RECV_VIDEO frameId=" + std::to_string(vh->frameId) +
                             " fragment=" + std::to_string(vh->fragmentIndex) + "/" + std::to_string(vh->totalFragments) +
                             " payloadBytes=" + std::to_string(vh->dataSize) +
                             " datagramBytes=" + std::to_string(len));
                    receiver.ProcessPacket(*vh, buf + sizeof(Protocol::VideoHeader));
                    lastFrameId = std::max((uint32_t)lastFrameId, vh->frameId);
                } else if (type == Protocol::PacketType::Audio && len >= (int)sizeof(Protocol::AudioHeader)) {
                    Protocol::AudioHeader* ah = (Protocol::AudioHeader*)buf;
                    Audio::AudioFrame pcmFrame;
                    if (audioDecoder.Decode(buf + sizeof(Protocol::AudioHeader), ah->dataSize, pcmFrame)) {
                        pcmFrame.timestamp = ah->captureTimestamp; // Overwrite with original capture timestamp for sync
                        auto framePtr = std::make_unique<Audio::AudioFrame>(std::move(pcmFrame));
                        audioJitterBuffer.PushFrame(std::move(framePtr));
                    }
                } else if (type == Protocol::PacketType::HandshakeResponse && len >= (int)sizeof(Protocol::HandshakeResponsePacket)) {
                    Protocol::HandshakeResponsePacket* hrp = (Protocol::HandshakeResponsePacket*)buf;
                    LOG_INFO("Session", "Received HandshakeResponse from " + senderIp + ". Approved: " + std::to_string((int)hrp->approved));
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
    LOG_INFO("Session", "Sending Handshake to " + std::string(config.hostIp));
    net.SendTo(&hp, sizeof(hp), config.hostIp, 5005);

    while (m_running && !handshakeApproved && !handshakeRejected) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHandshakeSend).count() >= 1) {
             LOG_INFO("Session", "Retransmitting Handshake to " + std::string(config.hostIp));
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
    uint32_t traceFrameCount = 0;
    bool requestKeyframe = false;
    uint32_t lastReportedMissingFrameId = 0;

    while (m_running) {
        try {
            auto now = std::chrono::steady_clock::now();

            // Detect missing frames and request keyframe if we have a gap that persists
            uint32_t nextExpected = receiver.GetNextExpectedFrameId();
            if (lastFrameId > nextExpected + 10 && nextExpected != lastReportedMissingFrameId) {
                LOG_WARN("Client", "Gap detected! Expected: " + std::to_string(nextExpected) + " Latest: " + std::to_string((uint32_t)lastFrameId));
                requestKeyframe = true;
                lastReportedMissingFrameId = nextExpected;
            }

            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFeedbackTime).count() >= 1000 || requestKeyframe) {
                Protocol::FeedbackHeader fh;
                fh.type = (uint8_t)Protocol::PacketType::Feedback;
                fh.lastReceivedFrameId = lastFrameId;

                auto stats = Profiler::getInstance().getStats("Network_LossRate");
                fh.lossRate = (float)stats.latest;

                stats = Profiler::getInstance().getStats("Network_RTT");
                fh.rttMs = (uint32_t)stats.latest;

                stats = Profiler::getInstance().getStats("Decode_Time");
                fh.avgDecodeTimeMs = (float)stats.avg / 1000.0f; // us to ms

                fh.requestKeyframe = requestKeyframe ? 1 : 0;

                net.SendTo(&fh, sizeof(fh), config.hostIp, 5005);
                lastFeedbackTime = now;
                requestKeyframe = false;
            }

            // Recovery logic: Scan for keyframes ahead of the current blocked read pointer.
            // This is done OUTSIDE GetNextFrame to bypass Head-of-Line blocking.
            uint32_t latestKeyframe = receiver.FindLatestAvailableKeyframe();
            if (latestKeyframe > 0 && latestKeyframe > receiver.GetNextExpectedFrameId()) {
                 LOG_INFO("Client", "Keyframe recovery detected ahead of blocked stream. Advancing to " + std::to_string(latestKeyframe));
                 receiver.ForceAdvanceTo(latestKeyframe);
                 jitterBuffer.Reset(latestKeyframe);
            }

            while (auto frame = receiver.GetNextFrame()) {
                if (traceFrameCount < 10) LOG_INFO("ClientTrace", "Frame Reassembled: " + std::to_string(frame->frameId));
                jitterBuffer.PushFrame(std::move(frame));
            }

            if (useRenderer) renderer.NewFrame();

            auto frame = jitterBuffer.PopFrame();
            if (frame) {
                uint32_t currentFid = frame->frameId;
                uint64_t decodeStart = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                void* outTexture = nullptr;
                int arrayIndex = 0;

                if (traceFrameCount < 10) LOG_INFO("ClientTrace", "Decoding Frame: " + std::to_string(currentFid));

                LOG_INFO("StreamTrace", "DECODER_IN frameId=" + std::to_string(frame->frameId) +
                         " bytes=" + std::to_string(frame->totalSize));
                if (decoder.DecodeFrame(frame->buffer.data(), frame->totalSize, &outTexture, &arrayIndex)) {
                    uint64_t decodeEnd = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                    LOG_INFO("StreamTrace", "DECODER_OUT frameId=" + std::to_string(frame->frameId) +
                             " texture=" + std::to_string(reinterpret_cast<uintptr_t>(outTexture)) +
                             " arrayIndex=" + std::to_string(arrayIndex) +
                             " decodeUs=" + std::to_string(decodeEnd - decodeStart));
                    Profiler::getInstance().recordTime("Decode_Time", (double)(decodeEnd - decodeStart));

                    uint64_t now = decodeEnd;
                    Profiler::getInstance().recordTime("EndToEnd_Latency", (double)(now - frame->captureTimestamp));

                    if (useRenderer && outTexture) {
                        if (traceFrameCount < 10) LOG_INFO("ClientTrace", "Rendering Frame: " + std::to_string(currentFid));
                        renderer.Render((ID3D11Texture2D*)outTexture, arrayIndex);
                    }

                    if (traceFrameCount < 10) traceFrameCount++;
                } else {
                    LOG_ERROR("StreamTrace", "DECODE_FAIL frameId=" + std::to_string(frame->frameId) +
                              " bytes=" + std::to_string(frame->totalSize));
                    requestKeyframe = true; // Request keyframe on decode failure
                    if (useRenderer) renderer.Render(nullptr, 0); // Redraw last frame on decode fail
                }
                receiver.ReturnToPool(std::move(frame));
            } else {
                if (useRenderer) renderer.Render(nullptr, 0); // Redraw last frame if no new frame popped
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            if (useRenderer) renderer.EndFrame();

            if (config.enableAudio) {
                // Pop and play only one frame per video frame to maintain a steady flow
                // and avoid burst playing everything which could cause audio-only jitter.
                if (auto audioFrame = audioJitterBuffer.PopFrame()) {
                    audioRenderer.Play(*audioFrame);
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Session", "Standard Exception in Client Loop: " + std::string(e.what()));
            m_running = false;
        } catch (...) {
            LOG_ERROR("Session", "Unknown Exception in Client Loop");
            m_running = false;
        }
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
