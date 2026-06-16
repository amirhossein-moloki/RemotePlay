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

PARSEC_API void Parsec_SetConnectionCallback(ParsecConnectionCallback callback) {
    SessionManager::getInstance().setConnectionCallback(callback);
}

PARSEC_API void Parsec_ApproveConnection(const char* ip, uint16_t port, bool approved) {
    SessionManager::getInstance().approveConnection(ip, port, approved);
}

PARSEC_API void Parsec_HandleMessage(uint32_t msg, uint64_t wParam, int64_t lParam) {
    SessionManager::getInstance().handleMessage(msg, wParam, lParam);
}

PARSEC_API void* Parsec_CreateClientWindow(const char* title, int width, int height) {
#ifdef _WIN32
    WNDCLASSA wc = { 0 };
    HINSTANCE hInstance = GetModuleHandleA(NULL);
    const char* className = "ParsecLiteStreamingWindow";

    if (!GetClassInfoA(hInstance, className, &wc)) {
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (msg == WM_DESTROY) {
                PostQuitMessage(0);
                return 0;
            }
            Parsec_HandleMessage(msg, (uint64_t)wParam, (int64_t)lParam);
            return DefWindowProcA(hwnd, msg, wParam, lParam);
        };
        wc.hInstance = hInstance;
        wc.lpszClassName = className;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassA(&wc);
    }

    HWND hwnd = CreateWindowExA(0, className, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInstance, NULL);
    return (void*)hwnd;
#else
    return nullptr;
#endif
}

}
