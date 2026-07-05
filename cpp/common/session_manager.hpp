#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <mutex>
#include "parsec_lite_api.h"
#include "crypto_manager.hpp"
#include "../ai/latency_predictor.hpp"
#include "../ai/intelligent_router.hpp"

class SessionManager {
public:
    static SessionManager& getInstance() {
        static SessionManager instance;
        return instance;
    }

    void startSession(ParsecConfig config);
    void stopSession();
    bool getTelemetry(ParsecTelemetry* outTelemetry);
    bool isRunning() const { return m_running; }

    void setConnectionCallback(ParsecConnectionCallback callback) { m_connectionCallback = callback; }
    void setErrorCallback(ParsecErrorCallback callback) { m_errorCallback = callback; }
    void reportError(ParsecError error, const std::string& message);
    void approveConnection(const std::string& ip, uint16_t port, bool approved);

    void handleMessage(uint32_t msg, uint64_t wParam, int64_t lParam);

    // WAN Negotiation Support
    struct ConnectivityCandidate {
        std::string ip;
        uint16_t port;
        uint8_t priority; // 0: LAN, 1: Srflx, 2: Relay
    };

    enum class SignalingState {
        IDLE,
        GATHERING_CANDIDATES,
        EXCHANGING,
        HOLE_PUNCHING,
        CONNECTED,
        FAILED
    };

private:
    SessionManager() = default;
    ~SessionManager() { stopSession(); }

    void runHost(ParsecConfig config);
    void runClient(ParsecConfig config);

    std::atomic<bool> m_running{false};
    std::thread m_hostThread;
    std::thread m_clientThread;
    ParsecConfig m_currentConfig;
    void* m_activeInputCapture = nullptr; // Client side
    ParsecConnectionCallback m_connectionCallback = nullptr;
    ParsecErrorCallback m_errorCallback = nullptr;

    std::mutex m_inputCaptureMutex;

    struct PendingClient {
        std::string ip;
        uint16_t port;
        std::string username;
        bool approved = false;
        bool waiting = true;
        std::vector<uint8_t> clientPublicKey;
        std::vector<ConnectivityCandidate> candidates;
        SignalingState signalingState = SignalingState::IDLE;
    };
    std::vector<PendingClient> m_pendingClients;
    std::mutex m_pendingClientsMutex;

    // AI Components
    std::unique_ptr<AI::LatencyPredictor> m_latencyPredictor;
    std::unique_ptr<AI::IntelligentRouter> m_intelligentRouter;
};
