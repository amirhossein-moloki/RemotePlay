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
#include "host/audio_capture.hpp"
#include "host/audio_encoder.hpp"
#include "host/input_injector.hpp"
#include "client/receiver.hpp"
#include "client/decoder_hw.hpp"
#include "client/audio_decoder.hpp"
#include "client/audio_renderer.hpp"
#include "client/input_capture.hpp"
#include "client/renderer_d3d11.hpp"
#include "client/jitter_buffer.hpp"
#include "clock_sync.hpp"
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

    m_latencyPredictor = std::make_unique<AI::LatencyPredictor>();
    m_intelligentRouter = std::make_unique<AI::IntelligentRouter>();
    m_cloudOptimizer = std::make_unique<AI::CloudOptimizer>();

    if (!Crypto::CryptoManager::Initialize()) {
        reportError(ParsecError::UNEXPECTED_ERROR, "Failed to initialize cryptography subsystem.");
        return;
    }

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
        std::unique_ptr<Host::AudioCapture> audioCapture;
        std::unique_ptr<Host::AudioEncoder> audioEncoder;
        LockFreeQueue<std::vector<Host::EncodedPacket>, 4> encodeQueue;
        uint32_t frameId = 0;
        uint32_t audioFrameId = 0;

        // Security
        std::vector<uint8_t> txKey;
        std::vector<uint8_t> rxKey;
        std::vector<uint8_t> clientPublicKey;
        uint64_t sessionId = 0;
        std::atomic<uint64_t> sendSequenceNumber{0};
        std::chrono::steady_clock::time_point lastActivity;
        mutable std::mutex stateMutex;

        struct CachedPacket {
            std::vector<uint8_t> data;
            uint32_t frameId;
            uint16_t fragmentIndex;
            bool occupied = false;
        };
        FixedRingBuffer<CachedPacket, 512> packetCache;
    };

    struct HostContext {
        Network::NetworkManager net;
        Host::CaptureDXGI capture;
        Host::InputInjector injector;
        std::mutex clientsMutex;
        std::vector<std::shared_ptr<ClientState>> clients;
        std::mutex initializingMutex;
        std::set<std::pair<std::string, uint16_t>> initializingClients;
        struct InitThread {
            std::thread thread;
            std::shared_ptr<std::atomic<bool>> finished;
        };
        std::mutex initThreadsMutex;
        std::vector<InitThread> initThreads;
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
    };
    auto ctx = std::make_shared<HostContext>();

    if (!ctx->net.Bind(config.selectedIp, 5005)) {
        std::string ipStr = config.selectedIp;
        reportError(ParsecError::NETWORK_BIND_FAILED, "Failed to bind to " + ipStr + ":5005. Check if the IP is correct and the port is not in use.");
        m_running = false;
        return;
    }

    // Step 1: Candidate Gathering (STUN)
    std::vector<ConnectivityCandidate> hostCandidates;
    hostCandidates.push_back({config.selectedIp, 5005, 0}); // Host Candidate

    std::string publicIp;
    uint16_t publicPort;
    if (ctx->net.GetPublicEndpoint("stun.l.google.com", 19302, publicIp, publicPort)) {
        LOG_INFO("Session", "Discovered Srflx candidate: " + publicIp + ":" + std::to_string(publicPort));
        hostCandidates.push_back({publicIp, publicPort, 1});
    }

    std::thread captureThread;

    captureThread = std::thread([this, ctx, config]() {
        if (!m_running) return;
        // Initialize capture on this thread for DXGI thread affinity
        if (!ctx->capture.Initialize()) {
            std::lock_guard<std::mutex> lock(ctx->initMutex);
            ctx->initFailed = true;
            ctx->initCv.notify_all();
            return;
        }

        // Detect resolution
        ID3D11Texture2D* testTex = nullptr;
        HRESULT hr = ctx->capture.AcquireFrame(&testTex);
        if (SUCCEEDED(hr)) {
            D3D11_TEXTURE2D_DESC desc;
            testTex->GetDesc(&desc);
            {
                std::lock_guard<std::mutex> lock(ctx->initMutex);
                ctx->capturedWidth = desc.Width;
                ctx->capturedHeight = desc.Height;

                // Apply resolution scale if requested
                if (config.resolutionScale > 0.0f && config.resolutionScale < 1.0f) {
                    ctx->capturedWidth = (int)(ctx->capturedWidth * config.resolutionScale);
                    ctx->capturedHeight = (int)(ctx->capturedHeight * config.resolutionScale);
                    // Ensure even dimensions for video encoding
                    ctx->capturedWidth &= ~1;
                    ctx->capturedHeight &= ~1;
                }

                ctx->initDone = true;
            }
            ctx->capture.ReleaseFrame();
            ctx->initCv.notify_all();
        } else {
            std::lock_guard<std::mutex> lock(ctx->initMutex);
            ctx->initFailed = true;
            ctx->initCv.notify_all();
            return;
        }

        uint32_t frameCount = 0;
        auto lastFpsCheck = std::chrono::high_resolution_clock::now();

        uint32_t lastProcessedFrameCount = 0;
        while (m_running) {
            // Session timeout check
            {
                auto now = std::chrono::steady_clock::now();
                std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                auto it = ctx->clients.begin();
                while (it != ctx->clients.end()) {
                    bool timedOut = false;
                    {
                        std::lock_guard<std::mutex> stateLock((*it)->stateMutex);
                        if (std::chrono::duration_cast<std::chrono::seconds>(now - (*it)->lastActivity).count() > 30) {
                            timedOut = true;
                        }
                    }
                    if (timedOut) {
                        LOG_INFO("Session", "Client " + (*it)->ip + " timed out.");
                        it = ctx->clients.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            frameCount++;
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsCheck).count();
            if (elapsed >= 1000) {
                Profiler::getInstance().recordValue("FPS", (double)frameCount * 1000.0 / elapsed);

                    // AI Infrastructure Optimization: Periodic Load Analysis
                    std::vector<AI::NodeStats> localNodes;
                    // In production, this would be a list of nodes in the current K8s region
                    localNodes.push_back({"local-host", 0.5f, 0.3f, (int)ctx->clients.size(), 32, 1});
                    auto signal = m_cloudOptimizer->AnalyzeRegionalLoad(1, localNodes);
                    if (signal.scaleUp) {
                        LOG_INFO("AI_Cloud", "AI recommends scaling up infrastructure: " + signal.reason);
                    }

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
                    std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                    if (!ctx->clients.empty()) {
                        Profiler::getInstance().recordValue("Host_Bitrate", (double)ctx->clients[0]->encoder->GetCurrentBitrate() / 1000.0);
                    }
                }

                frameCount = 0;
                lastFpsCheck = now;
            }

            ID3D11Texture2D* tex = nullptr;
            uint64_t captureTs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

            // Encode independently for each client to keep DXGI affinity and support per-client profiles
            HRESULT hr = ctx->capture.AcquireFrame(&tex);
            if (SUCCEEDED(hr)) {
                uint64_t endCaptureTs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                Profiler::getInstance().recordTime("Capture_Time", (double)(endCaptureTs - captureTs));

                D3D11_TEXTURE2D_DESC capturedDesc;
                tex->GetDesc(&capturedDesc);
                LOG_INFO("StreamTrace", "CAPTURE_OK frameAttempt=" + std::to_string(frameCount) +
                         " texture=" + std::to_string(capturedDesc.Width) + "x" + std::to_string(capturedDesc.Height) +
                         " captureUs=" + std::to_string(endCaptureTs - captureTs));

                {
                    std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                    for (auto& client : ctx->clients) {
                        uint32_t currentFid;
                        {
                            std::lock_guard<std::mutex> stateLock(client->stateMutex);
                            currentFid = client->frameId;
                        }
                        static thread_local std::vector<Host::EncodedPacket> packets;
                        packets.clear();
                        uint64_t encodeStart = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                        LOG_INFO("StreamTrace", "ENCODE_IN frameId=" + std::to_string(currentFid) +
                                 " client=" + client->ip + ":" + std::to_string(client->port));
                        if (client->encoder->EncodeFrame(tex, packets, ctx->packetPool)) {
                            uint64_t encodeEnd = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                            Profiler::getInstance().recordTime("Encode_Time", (double)(encodeEnd - encodeStart));
                            size_t encodedBytes = 0;
                            bool isHEVC = client->encoder->IsHEVC();
                            for (const auto& p : packets) {
                                if (p.packet) encodedBytes += p.packet->size;
                            }
                            LOG_INFO("StreamTrace", "ENCODE_OUT frameId=" + std::to_string(currentFid) +
                                     " packetCount=" + std::to_string(packets.size()) +
                                     " encodedBytes=" + std::to_string(encodedBytes) +
                                     " encodeUs=" + std::to_string(encodeEnd - encodeStart) +
                                     " codec=" + std::string(isHEVC ? "HEVC" : "H.264"));
                            for (auto& p : packets) {
                                p.captureTimestamp = endCaptureTs;
                                p.encodeStartTimestamp = encodeStart;
                                p.encodeEndTimestamp = encodeEnd;
                                if (isHEVC) p.isHEVC = true; // We need to add this to EncodedPacket or handle it here
                            }
                            if (!client->encodeQueue.push(std::move(packets))) {
                                LOG_ERROR("StreamTrace", "ENCODE_QUEUE_DROP frameId=" + std::to_string(currentFid));
                                for (auto& p : packets) ctx->packetPool.release(std::move(p.packet));
                            } else {
                                LOG_INFO("StreamTrace", "ENCODE_QUEUE_PUSH frameId=" + std::to_string(currentFid));
                            }
                        } else {
                            LOG_ERROR("StreamTrace", "ENCODE_FAIL frameId=" + std::to_string(currentFid));
                        }
                    }
                }
                ctx->capture.ReleaseFrame();
            } else if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
                // Ignore timeouts
                std::this_thread::yield();
            } else if (m_running) {
                // Fatal error or loss of access
                LOG_WARN("Capture", "Non-timeout failure in AcquireFrame (HR: " + std::to_string((long)hr) + "). Attempting re-init.");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (ctx->capture.Initialize()) {
                    std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                    for (auto& client : ctx->clients) {
                        LOG_INFO("Session", "Re-initializing encoder for client " + client->ip + " after device lost.");
                        client->encoder->Initialize(ctx->capturedWidth, ctx->capturedHeight, config.fps, ctx->capture.GetDevice(), config.useHardwareEncoding);
                        client->encoder->RequestKeyframe();
                    }
                }
            }
        }
    });


    std::thread packetizerThread = std::thread([this, ctx]() {
        while (m_running) {
            bool worked = false;
            std::vector<std::shared_ptr<ClientState>> activeClients;
            {
                std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                activeClients = ctx->clients;
            }

            for (auto& client : activeClients) {
                std::vector<Host::EncodedPacket> frames;
                if (client->encodeQueue.pop(frames)) {
                    worked = true;
                    uint32_t currentFid;
                    {
                        std::lock_guard<std::mutex> lock(client->stateMutex);
                        currentFid = client->frameId;
                    }
                    for (auto& frame : frames) {
                        uint16_t totalFrags = (uint16_t)((frame.packet->size + Protocol::MAX_UDP_PAYLOAD - 1) / Protocol::MAX_UDP_PAYLOAD);
                        for (uint16_t i = 0; i < totalFrags; ++i) {
                            auto udpPkt = ctx->udpPool.acquire();
                            if (!udpPkt) {
                                LOG_WARN("Session", "UDP Pool exhausted during packetization.");
                                break;
                            }
                            Protocol::VideoHeader* vh = (Protocol::VideoHeader*)udpPkt->data.data();
                            vh->type = (uint8_t)Protocol::PacketType::Video;
                            vh->frameId = currentFid;
                            vh->fragmentIndex = i;
                            vh->totalFragments = totalFrags;
                            vh->packetSequence = ctx->globalPacketSequence++;
                            vh->captureTimestamp = frame.captureTimestamp;
                            vh->encodeStartTimestamp = frame.encodeStartTimestamp;
                            vh->encodeEndTimestamp = frame.encodeEndTimestamp;
                            vh->flags = (frame.isKeyframe ? 0x01 : 0x00) | (frame.isHEVC ? 0x02 : 0x00);
                            vh->dataSize = (uint16_t)std::min((uint32_t)Protocol::MAX_UDP_PAYLOAD, (uint32_t)(frame.packet->size - i * Protocol::MAX_UDP_PAYLOAD));
                            memcpy(udpPkt->data.data() + sizeof(Protocol::VideoHeader), frame.packet->data.data() + (i * Protocol::MAX_UDP_PAYLOAD), vh->dataSize);

                            // Encrypt the Video Packet
                            auto securePkt = ctx->udpPool.acquire();
                            if (!securePkt) {
                                LOG_WARN("Session", "UDP Pool exhausted during encryption.");
                                ctx->udpPool.release(std::move(udpPkt));
                                break;
                            }

                            if (securePkt) {
                            Protocol::SecureHeader sh_stack;
                            sh_stack.type = (uint8_t)Protocol::PacketType::Secure;
                            sh_stack.sessionId = client->sessionId;
                            sh_stack.sequenceNumber = client->sendSequenceNumber++;
                            sh_stack.encryptedSize = sizeof(Protocol::VideoHeader) + vh->dataSize;

                            if (Crypto::CryptoManager::Encrypt(udpPkt->data.data(), sh_stack.encryptedSize,
                                                               (uint8_t*)&sh_stack.sessionId, sizeof(Protocol::SecureHeader) - 16,
                                                               sh_stack.sequenceNumber, client->txKey,
                                                                   securePkt->data.data() + sizeof(Protocol::SecureHeader),
                                                               sh_stack.authTag)) {
                                memcpy(securePkt->data.data(), &sh_stack, sizeof(Protocol::SecureHeader));
                                    securePkt->size = sizeof(Protocol::SecureHeader) + sh_stack.encryptedSize;
                                    ctx->udpPool.release(std::move(udpPkt));
                                    udpPkt = std::move(securePkt);
                                } else {
                                    ctx->udpPool.release(std::move(securePkt));
                                    // Fallback or error? For now, we drop if encryption fails
                                    ctx->udpPool.release(std::move(udpPkt));
                                    break;
                                }
                            } else {
                                ctx->udpPool.release(std::move(udpPkt));
                                break;
                            }

                            // Cache for potential retransmission
                            // Use bit-shifting for collision-resistant indexing (e.g., lower bits of frameId and fragmentIndex)
                            uint32_t cacheIdx = (vh->frameId << 6) ^ vh->fragmentIndex;
                            {
                                std::lock_guard<std::mutex> lock(client->stateMutex);
                                auto* cached = client->packetCache.get(cacheIdx);
                                cached->frameId = vh->frameId;
                                cached->fragmentIndex = vh->fragmentIndex;
                                cached->data.assign(securePkt->data.begin(), securePkt->data.begin() + securePkt->size);
                                cached->occupied = true;
                            }

                            if (!ctx->sendQueue.push({std::move(udpPkt), client->ip, client->port})) {
                                LOG_ERROR("StreamTrace", "SEND_QUEUE_DROP frameId=" + std::to_string(currentFid) +
                                          " fragment=" + std::to_string(i) + "/" + std::to_string(totalFrags));
                                ctx->udpPool.release(std::move(udpPkt));
                            } else {
                                LOG_INFO("StreamTrace", "SEND_QUEUE_PUSH frameId=" + std::to_string(currentFid) +
                                         " fragment=" + std::to_string(i) + "/" + std::to_string(totalFrags) +
                                         " bytes=" + std::to_string(vh->dataSize));
                            }
                        }
                        ctx->packetPool.release(std::move(frame.packet));
                    }
                    std::lock_guard<std::mutex> lock(client->stateMutex);
                    client->frameId++;
                }
            }

            if (!worked) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    std::thread senderThread = std::thread([this, ctx]() {
        const size_t MAX_BATCH = 64;
        Network::NetworkManager::BatchItem batch[MAX_BATCH];
        OutgoingPacket ops[MAX_BATCH];
        while (m_running || !ctx->sendQueue.empty()) {
            size_t count = 0;
            while (count < MAX_BATCH && ctx->sendQueue.pop(ops[count])) {
                batch[count].data = ops[count].packet->data.data();
                batch[count].size = ops[count].packet->size;
                batch[count].targetIp = ops[count].targetIp;
                batch[count].targetPort = ops[count].targetPort;
                count++;
            }
            if (count > 0) {
                int sendResult = ctx->net.SendBatch(batch, count);
                LOG_INFO("StreamTrace", "UDP_SEND_BATCH requested=" + std::to_string(count) +
                         " result=" + std::to_string(sendResult));
                if (sendResult < 0) {
                    LOG_ERROR("StreamTrace", "UDP_SEND_FAIL requested=" + std::to_string(count));
                }
                for (size_t i = 0; i < count; ++i) ctx->udpPool.release(std::move(ops[i].packet));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    std::thread receiverThread = std::thread([this, ctx, config]() {
        uint8_t buf[2048];
        std::string senderIp;
        uint16_t senderPort;
        while (m_running) {
            int len = ctx->net.ReceiveFrom(buf, sizeof(buf), senderIp, senderPort);
            while (len > 0) {
                Protocol::PacketType type = (Protocol::PacketType)buf[0];
                bool reprocess = false;
                if (type == Protocol::PacketType::CandidateDiscovery && len >= (int)sizeof(Protocol::CandidatePacket)) {
                    Protocol::CandidatePacket* cp = (Protocol::CandidatePacket*)buf;
                    LOG_INFO("Session", "Received Candidate from " + senderIp + ": " + cp->ip + ":" + std::to_string(cp->port));
                    // In a production P2P flow, the host would start hole punching back to this candidate
                    ctx->net.SendTo(buf, len, cp->ip, cp->port); // Reflective punch
                } else if (type == Protocol::PacketType::RelayData && len >= (int)sizeof(Protocol::RelayHeader)) {
                    Protocol::RelayHeader rh;
                    memcpy(&rh, buf, sizeof(Protocol::RelayHeader)); // Safe alignment copy

                    // Unwrap relay packet and process the inner content
                    if (len > (int)sizeof(Protocol::RelayHeader)) {
                        uint8_t* innerData = buf + sizeof(Protocol::RelayHeader);
                        int innerLen = len - sizeof(Protocol::RelayHeader);

                        // 1. Validate SessionID isolation at the relay layer before unwrapping
                        bool sessionMatch = false;
                        {
                            std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                            for (auto& c : ctx->clients) if (c->sessionId == rh.sessionId) { sessionMatch = true; break; }
                        }

                        if (sessionMatch) {
                            uint8_t localBuf[2048];
                            int copyLen = std::min(innerLen, (int)sizeof(localBuf));
                            memcpy(localBuf, innerData, copyLen);

                            if (localBuf[0] == (uint8_t)Protocol::PacketType::Secure) {
                                memcpy(buf, localBuf, copyLen);
                                len = copyLen;
                                reprocess = true;
                            }
                        } else {
                            LOG_WARN("Relay", "Dropping RelayData: SessionID mismatch (" + std::to_string(rh.sessionId) + ")");
                        }
                    }
                } else if (type == Protocol::PacketType::HandshakeSecure && len >= (int)sizeof(Protocol::HandshakeSecurePacket)) {
                    Protocol::HandshakeSecurePacket* hp = (Protocol::HandshakeSecurePacket*)buf;
                    std::string username(hp->username, strnlen(hp->username, sizeof(hp->username)));
                    LOG_INFO("Session", "Received Secure Handshake from " + senderIp + " (Username: " + username + ")");

                    std::shared_ptr<ClientState> existingClient;
                    {
                        std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                        for(auto& c : ctx->clients) if(c->ip == senderIp && c->port == senderPort) { existingClient = c; break; }
                    }

                    if (existingClient) {
                        // Check if it's the same client (same public key)
                        if (std::memcmp(existingClient->clientPublicKey.data(), hp->clientPublicKey, 32) == 0) {
                             LOG_INFO("Session", "Resending handshake response for active client " + senderIp);
                             // We don't store the host keypair in ClientState in Phase 1, but we can't re-gen or keys will mismatch.
                             // Actually, in a real production system we would store the host public key.
                             // For this fix, let's allow it to re-handshake IF we haven't seen activity recently,
                             // OR we must store the host public key to re-send.
                        }
                        // To keep it simple and correct: if already active, just drop and let them time out
                        // UNLESS we implement re-handshake.
                    }

                    if (!existingClient) {
                        bool isInitializing = false;
                        bool tooManyClients = false;
                        {
                            std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                            std::lock_guard<std::mutex> lock2(ctx->initializingMutex);
                            if (ctx->clients.size() + ctx->initializingClients.size() >= 32) {
                                tooManyClients = true;
                            } else if (ctx->initializingClients.count({senderIp, senderPort})) {
                                isInitializing = true;
                            } else {
                                ctx->initializingClients.insert({senderIp, senderPort});
                            }
                        }

                        if (tooManyClients) {
                            LOG_WARN("Session", "Rejecting handshake from " + senderIp + ": Too many concurrent connections/initializations (limit 32).");
                            Protocol::HandshakeSecureResponsePacket hrp;
                            hrp.type = (uint8_t)Protocol::PacketType::HandshakeSecureResponse;
                            hrp.approved = 0;
                            ctx->net.SendTo(&hrp, sizeof(hrp), senderIp, senderPort);
                        } else if (!isInitializing) {
                            std::vector<uint8_t> clientPubKey(hp->clientPublicKey, hp->clientPublicKey + 32);
                            std::lock_guard<std::mutex> lock(ctx->initThreadsMutex);

                            auto it = ctx->initThreads.begin();
                            while (it != ctx->initThreads.end()) {
                                if (it->finished->load()) {
                                    if (it->thread.joinable()) it->thread.join();
                                    it = ctx->initThreads.erase(it);
                                } else ++it;
                            }

                            auto finishedFlag = std::make_shared<std::atomic<bool>>(false);
                            ctx->initThreads.push_back({ std::thread([ctx, senderIp, senderPort, config, clientPubKey, username, this, finishedFlag]() {
                                if (!m_running) return;
                                auto newState = std::make_shared<ClientState>();
                                newState->ip = senderIp;
                                newState->port = senderPort;
                                newState->clientPublicKey = clientPubKey;
                                newState->encoder = std::make_unique<Host::EncoderManager>();
                                newState->audioCapture = std::make_unique<Host::AudioCapture>();
                                newState->audioEncoder = std::make_unique<Host::AudioEncoder>();
                                {
                                    std::lock_guard<std::mutex> lock(newState->stateMutex);
                                    newState->lastActivity = std::chrono::steady_clock::now();
                                }

                                // Key Exchange
                                auto hostKeyPair = Crypto::CryptoManager::GenerateX25519KeyPair();
                                if (!Crypto::CryptoManager::DeriveSessionKeys(hostKeyPair.publicKey, hostKeyPair.privateKey, clientPubKey,
                                                                              newState->rxKey, newState->txKey, true)) {
                                    LOG_ERROR("Session", "Key derivation failed for " + senderIp);
                                    std::lock_guard<std::mutex> lock(ctx->initializingMutex);
                                    ctx->initializingClients.erase({senderIp, senderPort});
                                    return;
                                }

                                uint64_t sessionID;
                                std::memcpy(&sessionID, Crypto::CryptoManager::GenerateRandomBytes(8).data(), 8);
                                newState->sessionId = sessionID;

                                if (newState->encoder->Initialize(ctx->capturedWidth, ctx->capturedHeight, config.fps, ctx->capture.GetDevice(), config.useHardwareEncoding)) {
                                    newState->audioEncoder->Initialize(48000, 2, 128000);
                                    newState->audioCapture->Initialize([this, ctx, newState](const float* data, size_t samples) {
                                        if (!m_running) return;
                                        std::vector<uint8_t> opus;
                                        if (newState->audioEncoder->Encode(data, samples, opus)) {
                                            auto udpPkt = ctx->udpPool.acquire();
                                            if (!udpPkt) {
                                                LOG_WARN("Session", "UDP Pool exhausted during audio capture.");
                                                return;
                                            }
                                            if (udpPkt) {
                                                Protocol::AudioHeader* ah = (Protocol::AudioHeader*)udpPkt->data.data();
                                                ah->type = (uint8_t)Protocol::PacketType::Audio;
                                                {
                                                    std::lock_guard<std::mutex> lock(newState->stateMutex);
                                                    ah->frameId = newState->audioFrameId++;
                                                }
                                                ah->timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                                                ah->dataSize = (uint16_t)opus.size();
                                                memcpy(udpPkt->data.data() + sizeof(Protocol::AudioHeader), opus.data(), opus.size());

                                                auto securePkt = ctx->udpPool.acquire();
                                                if (!securePkt) {
                                                    LOG_WARN("Session", "UDP Pool exhausted during audio encryption.");
                                                    ctx->udpPool.release(std::move(udpPkt));
                                                    return;
                                                }
                                                if (securePkt) {
                                                    Protocol::SecureHeader sh_stack;
                                                    sh_stack.type = (uint8_t)Protocol::PacketType::Secure;
                                                    sh_stack.sessionId = newState->sessionId;
                                                    sh_stack.sequenceNumber = newState->sendSequenceNumber++;
                                                    sh_stack.encryptedSize = sizeof(Protocol::AudioHeader) + ah->dataSize;

                                                    if (Crypto::CryptoManager::Encrypt(udpPkt->data.data(), sh_stack.encryptedSize,
                                                                                    (uint8_t*)&sh_stack.sessionId, sizeof(Protocol::SecureHeader) - 16,
                                                                                    sh_stack.sequenceNumber, newState->txKey,
                                                                                    securePkt->data.data() + sizeof(Protocol::SecureHeader),
                                                                                    sh_stack.authTag)) {
                                                        memcpy(securePkt->data.data(), &sh_stack, sizeof(Protocol::SecureHeader));
                                                        securePkt->size = sizeof(Protocol::SecureHeader) + sh_stack.encryptedSize;
                                                        ctx->sendQueue.push({std::move(securePkt), newState->ip, newState->port});
                                                    } else {
                                                        ctx->udpPool.release(std::move(securePkt));
                                                    }
                                                }
                                                ctx->udpPool.release(std::move(udpPkt));
                                            }
                                        }
                                    });
                                    newState->audioCapture->Start();

                                    {
                                        std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                                        ctx->clients.push_back(newState);
                                    }
                                    newState->encoder->RequestKeyframe();
                                    LOG_INFO("Session", "Securely initialized per-client encoder for " + senderIp + " (SessionID: " + std::to_string(sessionID) + ")");

                                    Protocol::HandshakeSecureResponsePacket hrp;
                                    hrp.type = (uint8_t)Protocol::PacketType::HandshakeSecureResponse;
                                    std::memcpy(hrp.hostPublicKey, hostKeyPair.publicKey.data(), 32);
                                    hrp.approved = 1;
                                    hrp.sessionId = sessionID;
                                    ctx->net.SendTo(&hrp, sizeof(hrp), senderIp, senderPort);
                                } else {
                                    LOG_ERROR("Session", "Failed to background initialize per-client encoder for " + senderIp + ". Rejecting connection.");
                                    this->reportError(ParsecError::ENCODER_INIT_FAILED, "Failed to initialize encoder for client " + senderIp);
                                    Protocol::HandshakeSecureResponsePacket hrp;
                                    hrp.type = (uint8_t)Protocol::PacketType::HandshakeSecureResponse;
                                    hrp.approved = 0;
                                    ctx->net.SendTo(&hrp, sizeof(hrp), senderIp, senderPort);
                                }

                                {
                                    std::lock_guard<std::mutex> lock(ctx->initializingMutex);
                                    ctx->initializingClients.erase({senderIp, senderPort});
                                }
                                finishedFlag->store(true);
                            }), finishedFlag });
                        }
                    }
                } else if (type == Protocol::PacketType::Secure && len >= (int)sizeof(Protocol::SecureHeader)) {
                    Protocol::SecureHeader sh;
                    memcpy(&sh, buf, sizeof(Protocol::SecureHeader));
                    std::shared_ptr<ClientState> targetClient;
                    {
                        std::lock_guard<std::mutex> lock(ctx->clientsMutex);
                        for (auto& c : ctx->clients) {
                            if (c->ip == senderIp && c->port == senderPort && c->sessionId == sh.sessionId) {
                                targetClient = c;
                                break;
                            }
                        }
                    }

                    if (targetClient) {
                        uint8_t decrypted[2048];
                        if (sh.encryptedSize > sizeof(decrypted)) continue;

                        if (Crypto::CryptoManager::Decrypt(buf + sizeof(Protocol::SecureHeader), sh.encryptedSize,
                                                           sh.authTag,
                                                           (uint8_t*)&sh.sessionId, sizeof(Protocol::SecureHeader) - 16,
                                                           sh.sequenceNumber, targetClient->rxKey,
                                                           decrypted)) {

                            {
                                std::lock_guard<std::mutex> lock(targetClient->stateMutex);
                                targetClient->lastActivity = std::chrono::steady_clock::now();

                                // Session Roaming: Identity is strictly verified via successful Decrypt above
                                // If the packet is valid but from a different IP/Port, update the client endpoint
                                if (targetClient->ip != senderIp || targetClient->port != senderPort) {
                                    LOG_INFO("Session", "Path Switch Verified for SID=" + std::to_string(targetClient->sessionId) +
                                             ". New Path: " + senderIp + ":" + std::to_string(senderPort));
                                    targetClient->ip = senderIp;
                                    targetClient->port = senderPort;
                                }
                            }

                            // Replay protection (Simple for Host in Phase 1)
                            // Note: Production should use the same sliding window as client.
                            static thread_local std::map<uint64_t, uint64_t> lastSeqs;
                            if (sh.sequenceNumber >= lastSeqs[sh.sessionId]) {
                                lastSeqs[sh.sessionId] = sh.sequenceNumber + 1;

                                Protocol::PacketType innerType = (Protocol::PacketType)decrypted[0];
                                if (innerType == Protocol::PacketType::Input && sh.encryptedSize >= (int)sizeof(Protocol::InputHeader)) {
                                    Protocol::InputHeader* ih = (Protocol::InputHeader*)decrypted;
                                    uint8_t* payload = decrypted + sizeof(Protocol::InputHeader);
                                    int payloadLen = sh.encryptedSize - (int)sizeof(Protocol::InputHeader);

                                    if (ih->inputType == (uint8_t)Protocol::InputType::Keyboard && payloadLen >= (int)sizeof(Protocol::KeyboardEvent)) {
                                        ctx->injector.InjectKeyboard(*(Protocol::KeyboardEvent*)payload);
                                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseMove && payloadLen >= (int)sizeof(Protocol::MouseMoveEvent)) {
                                        ctx->injector.InjectMouseMove(*(Protocol::MouseMoveEvent*)payload);
                                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseButton && payloadLen >= (int)sizeof(Protocol::MouseButtonEvent)) {
                                        ctx->injector.InjectMouseButton(*(Protocol::MouseButtonEvent*)payload);
                                    } else if (ih->inputType == (uint8_t)Protocol::InputType::MouseScroll && payloadLen >= (int)sizeof(Protocol::MouseScrollEvent)) {
                                        ctx->injector.InjectMouseScroll(*(Protocol::MouseScrollEvent*)payload);
                                    } else if (ih->inputType == (uint8_t)Protocol::InputType::GamepadStatus && payloadLen >= (int)sizeof(Protocol::GamepadStatusEvent)) {
                                        ctx->injector.HandleGamepadStatus(senderIp, *(Protocol::GamepadStatusEvent*)payload);
                                    } else if (ih->inputType == (uint8_t)Protocol::InputType::Gamepad && payloadLen >= (int)sizeof(Protocol::GamepadState)) {
                                        ctx->injector.InjectGamepad(senderIp, *(Protocol::GamepadState*)payload);
                                    }
                                } else if (innerType == Protocol::PacketType::Feedback && sh.encryptedSize >= (int)sizeof(Protocol::FeedbackHeader)) {
                                    Protocol::FeedbackHeader* fh = (Protocol::FeedbackHeader*)decrypted;
                                    Profiler::getInstance().recordValue("Network_LossRate", fh->lossRate);
                                    Profiler::getInstance().recordValue("Network_RTT", (double)fh->rttMs);

                                    // Feed AI Latency Predictor
                                    AI::NetworkMetrics metrics;
                                    metrics.rttMs = (float)fh->rttMs;
                                    metrics.packetLossRate = fh->lossRate;
                                    metrics.jitterMs = 0; // Derived if needed
                                    metrics.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                                    m_latencyPredictor->RecordMetrics(metrics);

                                    // Get AI Prediction
                                    auto prediction = m_latencyPredictor->Predict(200); // 200ms lookahead

                                    targetClient->encoder->UpdatePerformanceMetrics(fh->lossRate, -1.0f, fh->avgDecodeTimeMs, fh->targetBitrateKbps, &prediction);
                                    if (fh->requestKeyframe) {
                                        LOG_INFO("Session", "Keyframe requested by client " + senderIp);
                                        targetClient->encoder->RequestKeyframe();
                                    }
                                } else if (innerType == Protocol::PacketType::RetransmitRequest && sh.encryptedSize >= (int)sizeof(Protocol::RetransmitRequestPacket)) {
                                    Protocol::RetransmitRequestPacket* rrp = (Protocol::RetransmitRequestPacket*)decrypted;
                                    uint32_t cacheIdx = (rrp->frameId << 6) ^ rrp->fragmentIndex;
                                    std::lock_guard<std::mutex> lock(targetClient->stateMutex);
                                    auto* cached = targetClient->packetCache.get(cacheIdx);
                                    if (cached->occupied && cached->frameId == rrp->frameId && cached->fragmentIndex == rrp->fragmentIndex) {
                                        auto udpPkt = ctx->udpPool.acquire();
                                        if (!udpPkt) {
                                            LOG_WARN("Session", "UDP Pool exhausted during retransmission.");
                                        } else {
                                            memcpy(udpPkt->data.data(), cached->data.data(), cached->data.size());
                                            udpPkt->size = cached->data.size();
                                            ctx->sendQueue.push({ std::move(udpPkt), targetClient->ip, targetClient->port });
                                            LOG_INFO("Session", "Retransmitting frame=" + std::to_string(rrp->frameId) + " frag=" + std::to_string(rrp->fragmentIndex));
                                        }
                                    }
                                } else if (innerType == Protocol::PacketType::TimeSync && sh.encryptedSize >= (int)sizeof(Protocol::TimeSyncPacket)) {
                                    Protocol::TimeSyncPacket* tsp = (Protocol::TimeSyncPacket*)decrypted;
                                    Protocol::TimeSyncResponsePacket tsrp;
                                    tsrp.type = (uint8_t)Protocol::PacketType::TimeSync;
                                    tsrp.clientSendTimestamp = tsp->clientSendTimestamp;
                                    tsrp.hostReceiveTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                                    tsrp.hostSendTimestamp = tsrp.hostReceiveTimestamp; // Minimal processing delay

                                    uint8_t secureBuf[512];
                                    Protocol::SecureHeader sh_out;
                                    sh_out.type = (uint8_t)Protocol::PacketType::Secure;
                                    sh_out.sessionId = targetClient->sessionId;
                                    sh_out.sequenceNumber = targetClient->sendSequenceNumber++;
                                    sh_out.encryptedSize = sizeof(tsrp);

                                    if (Crypto::CryptoManager::Encrypt((uint8_t*)&tsrp, sizeof(tsrp),
                                                                    (uint8_t*)&sh_out.sessionId, sizeof(Protocol::SecureHeader) - 16,
                                                                    sh_out.sequenceNumber, targetClient->txKey,
                                                                    secureBuf + sizeof(Protocol::SecureHeader),
                                                                    sh_out.authTag)) {
                                        memcpy(secureBuf, &sh_out, sizeof(Protocol::SecureHeader));
                                        ctx->net.SendTo(secureBuf, sizeof(Protocol::SecureHeader) + sizeof(tsrp), senderIp, senderPort);
                                    }
                                }
                            }
                        }
                    }
                }

                if (reprocess) continue;
                break;
            }
        }
    });

    // Wait for capture initialization
    {
        std::unique_lock<std::mutex> lock(ctx->initMutex);
        ctx->initCv.wait(lock, [&] { return ctx->initDone || ctx->initFailed || !m_running; });
        if (ctx->initFailed || !m_running) {
            if (ctx->initFailed) reportError(ParsecError::HARDWARE_INIT_FAILED, "Capture initialization failed. Please check your GPU drivers and ensure you are not in a RDP session.");
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

    if (captureThread.joinable()) captureThread.join();
    if (packetizerThread.joinable()) packetizerThread.join();
    if (senderThread.joinable()) senderThread.join();
    if (receiverThread.joinable()) receiverThread.join();

    {
        std::lock_guard<std::mutex> lock(ctx->initThreadsMutex);
        for (auto& t : ctx->initThreads) {
            if (t.thread.joinable()) {
                // Ensure the thread knows to stop if it hasn't finished
                t.finished->store(true);
                t.thread.join();
            }
        }
        ctx->initThreads.clear();
    }
}

void SessionManager::runClient(ParsecConfig config) {
    LOG_INFO("Session", "Starting Client Session");

    Network::NetworkManager net;
    if (!net.Bind(config.selectedIp, 0)) {
        reportError(ParsecError::NETWORK_BIND_FAILED, "Failed to bind to local interface " + std::string(config.selectedIp));
        m_running = false;
        return;
    }

    // Step 1: Candidate Gathering (STUN)
    std::vector<ConnectivityCandidate> clientCandidates;
    clientCandidates.push_back({config.selectedIp, 0, 0}); // Local Candidate

    std::string publicIp;
    uint16_t publicPort;
    std::vector<Network::NetworkManager::StunServer> stunServers = {
        {"stun.l.google.com", 19302},
        {"stun1.l.google.com", 19302},
        {"stun2.l.google.com", 19302}
    };
    if (net.GetPublicEndpointParallel(stunServers, publicIp, publicPort)) {
        LOG_INFO("Session", "Discovered Srflx candidate: " + publicIp + ":" + std::to_string(publicPort));
        clientCandidates.push_back({publicIp, publicPort, 1});
    }

    // Security Context (Client)
    std::vector<uint8_t> rxKey;
    std::vector<uint8_t> txKey;
    uint64_t sessionId = 0;
    std::atomic<uint64_t> sendSequenceNumber{0};
    auto clientKeyPair = Crypto::CryptoManager::GenerateX25519KeyPair();

    Client::Receiver receiver;
    Client::JitterBuffer jitterBuffer(10);
    Client::DecoderHW decoder;
    Client::RendererD3D11 renderer;
    Client::AudioDecoder audioDecoder;
    Client::AudioRenderer audioRenderer;
    Common::ClockSync clockSync;

    std::string currentHostIp = config.hostIp;
    uint16_t currentHostPort = 5005;

    std::string hostIpStr(config.hostIp);
    bool isLoopback = (hostIpStr == "127.0.0.1" || hostIpStr == "localhost" || hostIpStr == "::1");

    Client::InputCapture input([&, isLoopback, config, &txKey, &sessionId, &sendSequenceNumber, &currentHostIp, &currentHostPort](const uint8_t* data, size_t size) {
        if (!isLoopback && sessionId != 0) {
            uint8_t secureBuf[2048];
            if (size + sizeof(Protocol::SecureHeader) > sizeof(secureBuf)) return;

            Protocol::SecureHeader sh_stack;
            sh_stack.type = (uint8_t)Protocol::PacketType::Secure;
            sh_stack.sessionId = sessionId;
            sh_stack.sequenceNumber = sendSequenceNumber++;
            sh_stack.encryptedSize = (uint16_t)size;

            if (Crypto::CryptoManager::Encrypt(data, size,
                                               (uint8_t*)&sh_stack.sessionId, sizeof(Protocol::SecureHeader) - 16,
                                               sh_stack.sequenceNumber, txKey,
                                               secureBuf + sizeof(Protocol::SecureHeader),
                                               sh_stack.authTag)) {
                memcpy(secureBuf, &sh_stack, sizeof(Protocol::SecureHeader));
                net.SendTo(secureBuf, sizeof(Protocol::SecureHeader) + size, currentHostIp, currentHostPort);
            }
        }
    });

    bool useRenderer = (config.windowHandle != nullptr);
    if (useRenderer) {
        input.RegisterDevices((HWND)config.windowHandle);
        std::lock_guard<std::mutex> lock(m_inputCaptureMutex);
        m_activeInputCapture = &input;
    }
    if (useRenderer) {
        int initW = config.targetWidth > 0 ? config.targetWidth : 1920;
        int initH = config.targetHeight > 0 ? config.targetHeight : 1080;
        if (!renderer.Initialize((HWND)config.windowHandle, initW, initH)) {
            LOG_ERROR("Session", "Failed to initialize renderer. Stopping session.");
            reportError(ParsecError::RENDERER_INIT_FAILED, "Renderer initialization failed. Please check your GPU drivers and system compatibility.");
            m_running = false;
            return;
        } else if (!decoder.Initialize(renderer.GetDevice(), config.useHardwareEncoding)) {
            LOG_ERROR("Session", "Failed to initialize decoder. Stopping session.");
            reportError(ParsecError::DECODER_INIT_FAILED, "Hardware decoder initialization failed. Please ensure your GPU supports H.264 or HEVC decoding.");
            m_running = false;
            return;
        }
    } else {
        if (!decoder.Initialize(nullptr, config.useHardwareEncoding)) {
            LOG_ERROR("Session", "Failed to initialize headless decoder.");
            reportError(ParsecError::DECODER_INIT_FAILED, "Headless decoder initialization failed.");
            m_running = false;
            return;
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
            while (len > 0) {
                Protocol::PacketType type = (Protocol::PacketType)buf[0];
                bool reprocess = false;
                if (type == Protocol::PacketType::CandidateDiscovery && len >= (int)sizeof(Protocol::CandidatePacket)) {
                    Protocol::CandidatePacket* cp = (Protocol::CandidatePacket*)buf;
                    LOG_INFO("Session", "Received Host Candidate: " + std::string(cp->ip));
                    // Client would add this to the list of targets to try for handshake
                } else if (type == Protocol::PacketType::RelayData && len >= (int)sizeof(Protocol::RelayHeader)) {
                     Protocol::RelayHeader rh;
                     memcpy(&rh, buf, sizeof(Protocol::RelayHeader));

                     // Relay unwrap on client
                     if (len > (int)sizeof(Protocol::RelayHeader) && rh.sessionId == sessionId) {
                         uint8_t* inner = buf + sizeof(Protocol::RelayHeader);
                         int innerLen = len - sizeof(Protocol::RelayHeader);
                         if (inner[0] == (uint8_t)Protocol::PacketType::Secure) {
                             // Move inner data to start of buf and re-trigger Secure logic
                             memmove(buf, inner, innerLen);
                             len = innerLen;
                             reprocess = true;
                         }
                     }
                } else if (type == Protocol::PacketType::Secure && len >= (int)sizeof(Protocol::SecureHeader)) {
                    Protocol::SecureHeader* sh = (Protocol::SecureHeader*)buf;
                    if (sh->sessionId == sessionId && sessionId != 0) {
                        uint8_t decrypted[2048];
                        if (sh->encryptedSize > sizeof(decrypted)) continue;

                        if (Crypto::CryptoManager::Decrypt(buf + sizeof(Protocol::SecureHeader), sh->encryptedSize,
                                                           sh->authTag,
                                                           (uint8_t*)&sh->sessionId, sizeof(Protocol::SecureHeader) - 16,
                                                           sh->sequenceNumber, rxKey,
                                                           decrypted)) {

                            // Check sequence number for replay protection
                            if (receiver.ValidateSequence(sh->sequenceNumber)) {
                                Protocol::PacketType innerType = (Protocol::PacketType)decrypted[0];
                                if (innerType == Protocol::PacketType::Video && sh->encryptedSize >= sizeof(Protocol::VideoHeader)) {
                                    Protocol::VideoHeader* vh = (Protocol::VideoHeader*)decrypted;
                                    receiver.ProcessPacket(*vh, decrypted + sizeof(Protocol::VideoHeader));
                                    lastFrameId = std::max((uint32_t)lastFrameId, vh->frameId);
                                } else if (innerType == Protocol::PacketType::Audio && sh->encryptedSize >= sizeof(Protocol::AudioHeader)) {
                                    Protocol::AudioHeader* ah = (Protocol::AudioHeader*)decrypted;
                                    std::vector<float> pcm;
                                    if (audioDecoder.Decode(decrypted + sizeof(Protocol::AudioHeader), ah->dataSize, pcm)) {
                                        audioRenderer.PushSamples(pcm.data(), pcm.size());
                                    }
                                } else if (innerType == Protocol::PacketType::TimeSync && sh->encryptedSize >= sizeof(Protocol::TimeSyncResponsePacket)) {
                                    Protocol::TimeSyncResponsePacket* tsrp = (Protocol::TimeSyncResponsePacket*)decrypted;
                                    clockSync.ProcessResponse(tsrp->clientSendTimestamp, tsrp->hostReceiveTimestamp, tsrp->hostSendTimestamp,
                                                              std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count());
                                }
                            }
                        }
                    }
                } else if (type == Protocol::PacketType::HandshakeSecureResponse && len >= (int)sizeof(Protocol::HandshakeSecureResponsePacket)) {
                    Protocol::HandshakeSecureResponsePacket* hrp = (Protocol::HandshakeSecureResponsePacket*)buf;
                    LOG_INFO("Session", "Received HandshakeSecureResponse from " + senderIp + ". Approved: " + std::to_string((int)hrp->approved));
                    if (hrp->approved) {
                        std::vector<uint8_t> hostPubKey(hrp->hostPublicKey, hrp->hostPublicKey + 32);
                        if (Crypto::CryptoManager::DeriveSessionKeys(clientKeyPair.publicKey, clientKeyPair.privateKey, hostPubKey,
                                                                     rxKey, txKey, false)) {
                            sessionId = hrp->sessionId;
                            currentHostIp = senderIp;
                            currentHostPort = senderPort;
                            handshakeApproved = true;
                            LOG_INFO("Session", "Verified Path: " + currentHostIp + ":" + std::to_string(currentHostPort));
                        } else {
                            LOG_ERROR("Session", "Key derivation failed on client");
                        }
                    }
                    else handshakeRejected = true;
                }

                if (reprocess) continue;
                break;
            }
        }
    });

    // Step 2: Hole Punching / Candidate Exchange (Simulated Signaling)
    for (const auto& cand : clientCandidates) {
        Protocol::CandidatePacket cp;
        cp.type = (uint8_t)Protocol::PacketType::CandidateDiscovery;
        strncpy(cp.ip, cand.ip.c_str(), sizeof(cp.ip) - 1);
        cp.port = cand.port;
        cp.priority = cand.priority;
        net.SendTo(&cp, sizeof(cp), config.hostIp, 5005);
    }

    Protocol::HandshakeSecurePacket hp;
    hp.type = (uint8_t)Protocol::PacketType::HandshakeSecure;
    std::memcpy(hp.clientPublicKey, clientKeyPair.publicKey.data(), 32);
    strncpy(hp.username, config.username, sizeof(hp.username) - 1);
    hp.username[sizeof(hp.username) - 1] = '\0';

    auto startHandshake = std::chrono::steady_clock::now();
    auto lastHandshakeSend = std::chrono::steady_clock::now();
    LOG_INFO("Session", "Sending Secure Handshake to " + std::string(config.hostIp));

    // WAN Connection Strategy: Attempt all discovered candidates (Intelligent Router optimized)
    auto attemptHandshake = [&]() {
        // 1. Direct/Configured Path
        net.SendTo(&hp, sizeof(hp), config.hostIp, 5005);

        // 2. Parallel Probing across all candidates
        std::vector<AI::RouteCandidate> aiCandidates;
        for (const auto& cand : clientCandidates) {
            float baseLat = (cand.priority == 0) ? 50.0f : ((cand.priority == 1) ? 100.0f : 200.0f);
            aiCandidates.push_back({ cand.ip, cand.ip, cand.port, baseLat, 0, 0 });
        }

        auto ranked = m_intelligentRouter->RankRoutes(aiCandidates);
        for (const auto& score : ranked) {
            for (const auto& cand : clientCandidates) {
                if (cand.ip == score.id) {
                    // Send Handshake and a Hole Punch packet
                    net.SendTo(&hp, sizeof(hp), cand.ip, cand.port);

                    Protocol::CandidatePacket hpp;
                    hpp.type = (uint8_t)Protocol::PacketType::CandidateDiscovery;
                    strncpy(hpp.ip, publicIp.c_str(), sizeof(hpp.ip)-1);
                    hpp.port = publicPort;
                    net.SendTo(&hpp, sizeof(hpp), cand.ip, cand.port);
                }
            }
        }
    };

    attemptHandshake();

    int retryCount = 0;
    bool relayFallbackTriggered = false;

    while (m_running && !handshakeApproved && !handshakeRejected) {
        auto now = std::chrono::steady_clock::now();
        auto elapsedTotal = std::chrono::duration_cast<std::chrono::seconds>(now - startHandshake).count();

        // Aggressive Relay Fallback: If P2P hasn't succeeded in 3 seconds, prioritize Relay
        if (elapsedTotal >= 3 && !relayFallbackTriggered) {
            LOG_WARN("Session", "P2P handshake lagging. Triggering aggressive Relay fallback.");
            relayFallbackTriggered = true;
            // Force a re-attempt immediately with relay priority
        }

        // Exponential backoff for handshake retries
        int interval = std::min(5, (1 << retryCount));

        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHandshakeSend).count() >= interval || (relayFallbackTriggered && retryCount == 0)) {
             LOG_INFO("Session", "Retransmitting Handshake (Retry #" + std::to_string(retryCount) + ")...");

             // If relay fallback triggered, we might want to prioritize relay candidates here
             // For this foundation, attemptHandshake already probes all, but we ensure it's called
             attemptHandshake();
             lastHandshakeSend = now;
             retryCount++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (elapsedTotal > 30) {
            LOG_ERROR("Session", "Handshake timeout after 30 seconds.");
            break;
        }
    }

    if (!handshakeApproved) {
        LOG_ERROR("Session", handshakeRejected ? "Connection rejected by host" : "Handshake failed");
        reportError(handshakeRejected ? ParsecError::HANDSHAKE_REJECTED : ParsecError::HANDSHAKE_TIMEOUT,
                    handshakeRejected ? "The host rejected your connection request." : "Failed to connect to host: Handshake timeout. Check the IP and network connectivity.");
        m_running = false;
        {
            std::lock_guard<std::mutex> lock(m_inputCaptureMutex);
            m_activeInputCapture = nullptr;
        }
        if (netThread.joinable()) netThread.join();
        return;
    }

    LOG_INFO("Session", "Handshake approved, starting stream");

    auto lastFeedbackTime = std::chrono::steady_clock::now();
    uint32_t traceFrameCount = 0;
    bool requestKeyframe = false;
    uint32_t lastReportedMissingFrameId = 0;

    auto lastTimeSync = std::chrono::steady_clock::now();

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
                if (sessionId != 0) {
                    // AI Routing: Periodically re-evaluate network path
                    std::vector<AI::RouteCandidate> routes;
                    // Current Path
                    routes.push_back({"current", currentHostIp, currentHostPort, (float)Profiler::getInstance().getStats("Network_RTT").latest, (float)Profiler::getInstance().getStats("Network_LossRate").latest, 0, 0});

                    // Probe for better routes (simulated from candidates)
                    for (const auto& cand : clientCandidates) {
                        if (cand.ip != currentHostIp) {
                            routes.push_back({cand.ip, cand.ip, cand.port, 100.0f, 0.01f, 0, 0}); // Heuristic probe
                        }
                    }

                    auto best = m_intelligentRouter->SelectBestRoute(routes);
                    if (best.id != "current") {
                        AI::RouteCandidate current = routes[0];
                        if (m_intelligentRouter->ShouldSwitchRoute(current, best)) {
                            LOG_INFO("AI_Router", "AI suggesting path switch to " + best.ip + ":" + std::to_string(best.port));
                            currentHostIp = best.ip;
                            currentHostPort = best.port;
                        }
                    }

                    // Send Feedback
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

                    uint8_t secureBuf[512];
                        Protocol::SecureHeader sh_stack;
                        sh_stack.type = (uint8_t)Protocol::PacketType::Secure;
                        sh_stack.sessionId = sessionId;
                        sh_stack.sequenceNumber = sendSequenceNumber++;
                        sh_stack.encryptedSize = sizeof(fh);

                    if (Crypto::CryptoManager::Encrypt((uint8_t*)&fh, sizeof(fh),
                                                           (uint8_t*)&sh_stack.sessionId, sizeof(Protocol::SecureHeader) - 16,
                                                           sh_stack.sequenceNumber, txKey,
                                                       secureBuf + sizeof(Protocol::SecureHeader),
                                                           sh_stack.authTag)) {
                            memcpy(secureBuf, &sh_stack, sizeof(Protocol::SecureHeader));
                        net.SendTo(secureBuf, sizeof(Protocol::SecureHeader) + sizeof(fh), currentHostIp, currentHostPort);
                    }

                    lastFeedbackTime = now;
                    requestKeyframe = false;
                }
            }

            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimeSync).count() >= 20) {
                auto nacks = receiver.GetPendingNACKs();
                for (const auto& nack : nacks) {
                    Protocol::RetransmitRequestPacket rrp;
                    rrp.type = (uint8_t)Protocol::PacketType::RetransmitRequest;
                    rrp.frameId = nack.frameId;
                    rrp.fragmentIndex = nack.fragmentIndex;

                    uint8_t secureBuf[512];
                    Protocol::SecureHeader sh_stack;
                    sh_stack.type = (uint8_t)Protocol::PacketType::Secure;
                    sh_stack.sessionId = sessionId;
                    sh_stack.sequenceNumber = sendSequenceNumber++;
                    sh_stack.encryptedSize = sizeof(rrp);

                    if (Crypto::CryptoManager::Encrypt((uint8_t*)&rrp, sizeof(rrp),
                                                       (uint8_t*)&sh_stack.sessionId, sizeof(Protocol::SecureHeader) - 16,
                                                       sh_stack.sequenceNumber, txKey,
                                                       secureBuf + sizeof(Protocol::SecureHeader),
                                                       sh_stack.authTag)) {
                        memcpy(secureBuf, &sh_stack, sizeof(Protocol::SecureHeader));
                        net.SendTo(secureBuf, sizeof(Protocol::SecureHeader) + sizeof(rrp), currentHostIp, currentHostPort);
                    }
                }
            }

            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastTimeSync).count() >= 2) {
                if (sessionId != 0) {
                    Protocol::TimeSyncPacket tsp;
                    tsp.type = (uint8_t)Protocol::PacketType::TimeSync;
                    tsp.clientSendTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

                    uint8_t secureBuf[512];
                    Protocol::SecureHeader sh_stack;
                    sh_stack.type = (uint8_t)Protocol::PacketType::Secure;
                    sh_stack.sessionId = sessionId;
                    sh_stack.sequenceNumber = sendSequenceNumber++;
                    sh_stack.encryptedSize = sizeof(tsp);

                    if (Crypto::CryptoManager::Encrypt((uint8_t*)&tsp, sizeof(tsp),
                                                       (uint8_t*)&sh_stack.sessionId, sizeof(Protocol::SecureHeader) - 16,
                                                       sh_stack.sequenceNumber, txKey,
                                                       secureBuf + sizeof(Protocol::SecureHeader),
                                                       sh_stack.authTag)) {
                        memcpy(secureBuf, &sh_stack, sizeof(Protocol::SecureHeader));
                        net.SendTo(secureBuf, sizeof(Protocol::SecureHeader) + sizeof(tsp), currentHostIp, currentHostPort);
                    }
                    lastTimeSync = now;
                }
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

            uint64_t syncTime = clockSync.IsSynchronized() ? clockSync.GetHostTime(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()) : 0;
            auto frame = jitterBuffer.PopFrame(syncTime);
            if (frame) {
                uint32_t currentFid = frame->frameId;
                uint64_t decodeStart = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                void* outTexture = nullptr;
                int arrayIndex = 0;

                if (traceFrameCount < 10) LOG_INFO("ClientTrace", "Decoding Frame: " + std::to_string(currentFid));

                LOG_INFO("StreamTrace", "DECODER_IN frameId=" + std::to_string(frame->frameId) +
                         " bytes=" + std::to_string(frame->totalSize) + " hevc=" + std::to_string(frame->isHEVC));
                if (decoder.DecodeFrame(frame->buffer.data(), frame->totalSize, &outTexture, &arrayIndex, frame->isHEVC)) {
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
                        if (!renderer.Render((ID3D11Texture2D*)outTexture, arrayIndex)) {
                            LOG_WARN("Session", "Renderer lost during Render, attempting re-init.");
                            renderer.Shutdown();
                            decoder.Shutdown();
                            int rw = config.targetWidth > 0 ? config.targetWidth : 1920;
                            int rh = config.targetHeight > 0 ? config.targetHeight : 1080;
                            if (renderer.Initialize((HWND)config.windowHandle, rw, rh)) {
                                decoder.Initialize(renderer.GetDevice(), config.useHardwareEncoding);
                            }
                        }
                    }

                    if (traceFrameCount < 10) traceFrameCount++;
                } else {
                    LOG_ERROR("StreamTrace", "DECODE_FAIL frameId=" + std::to_string(frame->frameId) +
                              " bytes=" + std::to_string(frame->totalSize));
                    requestKeyframe = true; // Request keyframe on decode failure
                    if (useRenderer) {
                        if (!renderer.Render(nullptr, 0)) {
                            LOG_WARN("Session", "Renderer lost during Render (null), attempting re-init.");
                            renderer.Shutdown();
                            decoder.Shutdown();
                            int rw = config.targetWidth > 0 ? config.targetWidth : 1920;
                            int rh = config.targetHeight > 0 ? config.targetHeight : 1080;
                            if (renderer.Initialize((HWND)config.windowHandle, rw, rh)) {
                                decoder.Initialize(renderer.GetDevice(), config.useHardwareEncoding);
                            }
                        }
                    }
                }
                receiver.ReturnToPool(std::move(frame));
            } else {
                if (useRenderer) {
                    if (!renderer.Render(nullptr, 0)) {
                        LOG_WARN("Session", "Renderer lost during idle Render, attempting re-init.");
                        renderer.Shutdown();
                        decoder.Shutdown();
                        int rw = config.targetWidth > 0 ? config.targetWidth : 1920;
                        int rh = config.targetHeight > 0 ? config.targetHeight : 1080;
                        if (renderer.Initialize((HWND)config.windowHandle, rw, rh)) {
                            decoder.Initialize(renderer.GetDevice(), config.useHardwareEncoding);
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            if (useRenderer) {
                if (!renderer.EndFrame()) {
                    LOG_WARN("Session", "Renderer lost during EndFrame, attempting re-init.");
                    renderer.Shutdown();
                    decoder.Shutdown();
                    int rw = config.targetWidth > 0 ? config.targetWidth : 1920;
                    int rh = config.targetHeight > 0 ? config.targetHeight : 1080;
                    if (renderer.Initialize((HWND)config.windowHandle, rw, rh)) {
                        decoder.Initialize(renderer.GetDevice(), config.useHardwareEncoding);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::string msg = "Standard Exception in Client Loop: " + std::string(e.what());
            LOG_ERROR("Session", msg);
            reportError(ParsecError::UNEXPECTED_ERROR, msg);
            m_running = false;
        } catch (...) {
            LOG_ERROR("Session", "Unknown Exception in Client Loop");
            reportError(ParsecError::UNEXPECTED_ERROR, "Unknown critical exception in client loop.");
            m_running = false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_inputCaptureMutex);
        m_activeInputCapture = nullptr;
    }
    if (netThread.joinable()) netThread.join();
}

void SessionManager::handleMessage(uint32_t msg, uint64_t wParam, int64_t lParam) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(m_inputCaptureMutex);
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
void SessionManager::handleMessage(uint32_t msg, uint64_t wParam, int64_t lParam) {}
#endif
