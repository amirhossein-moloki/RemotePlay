#include "parsec_lite_api.h"
#include "session_manager.hpp"
#include "logger.hpp"
#include "config.hpp"

extern "C" {

PARSEC_API bool Parsec_Initialize() {
    Logger::getInstance().init("parsec-lite-core.log");
    Config::getInstance().load("config.ini");
    return true;
}

PARSEC_API void Parsec_StartSession(ParsecConfig config) {
    SessionManager::getInstance().startSession(config);
}

PARSEC_API void Parsec_StopSession() {
    SessionManager::getInstance().stopSession();
}

PARSEC_API bool Parsec_GetTelemetry(ParsecTelemetry* outTelemetry) {
    return SessionManager::getInstance().getTelemetry(outTelemetry);
}

PARSEC_API void Parsec_Shutdown() {
    SessionManager::getInstance().stopSession();
}

}
