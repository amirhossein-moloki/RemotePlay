#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include "parsec_lite_api.h"

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

private:
    SessionManager() = default;
    ~SessionManager() { stopSession(); }

    void runHost(ParsecConfig config);
    void runClient(ParsecConfig config);

    std::atomic<bool> m_running{false};
    std::thread m_sessionThread;
    ParsecConfig m_currentConfig;
};
