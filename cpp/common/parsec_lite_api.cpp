#include "parsec_lite_api.h"
#include <string>
#include <thread>
#include <atomic>
#include "../common/logger.hpp"
#include "../common/config.hpp"

// Placeholder for actual implementation integration
// In a full implementation, the logic from main.cpp would be moved here
// and encapsulated in a singleton or session manager.

static std::atomic<bool> g_sessionRunning{false};
static std::thread g_sessionThread;

extern "C" {

PARSEC_API bool Parsec_Initialize() {
    Logger::getInstance().init("parsec-lite-core.log");
    return true;
}

PARSEC_API void Parsec_StartSession(ParsecConfig config) {
    if (g_sessionRunning) return;

    g_sessionRunning = true;
    g_sessionThread = std::thread([config]() {
        LOG_INFO("API", "Starting session in " + std::string(config.isHost ? "Host" : "Client") + " mode");
        // Logic from RunHost/RunClient would be invoked here
        while (g_sessionRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        LOG_INFO("API", "Session stopped");
    });
}

PARSEC_API void Parsec_StopSession() {
    g_sessionRunning = false;
    if (g_sessionThread.joinable()) {
        g_sessionThread.join();
    }
}

PARSEC_API bool Parsec_GetTelemetry(ParsecTelemetry* outTelemetry) {
    if (!outTelemetry) return false;

    // Mock telemetry for demonstration
    outTelemetry->e2eLatency = 15.5f;
    outTelemetry->fps = 60.0f;
    outTelemetry->bitrateMbps = 8.5f;
    outTelemetry->lossRate = 0.01f;

    return true;
}

PARSEC_API void Parsec_Shutdown() {
    Parsec_StopSession();
}

}
