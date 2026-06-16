#pragma once
#include <stdint.h>

#ifdef _WIN32
#ifdef PARSEC_LITE_EXPORT
#define PARSEC_API __declspec(dllexport)
#else
#define PARSEC_API __declspec(dllimport)
#endif
#else
#define PARSEC_API
#endif

extern "C" {

struct ParsecConfig {
    char selectedIp[64];
    char hostIp[64];
    int bitrate;
    int fps;
    bool isHost;
    bool useHardwareEncoding;
    void* windowHandle; // HWND on Windows
};

struct ParsecTelemetry {
    float e2eLatency;
    float captureTime;
    float encodeTime;
    float networkTime;
    float decodeTime;
    float fps;
    float lossRate;
    float rtt;
    float bitrateMbps;
};

PARSEC_API bool Parsec_Initialize();
PARSEC_API void Parsec_StartSession(ParsecConfig config);
PARSEC_API void Parsec_StopSession();
PARSEC_API bool Parsec_GetTelemetry(ParsecTelemetry* outTelemetry);
PARSEC_API void Parsec_Shutdown();

// UI Support
PARSEC_API void Parsec_HandleMessage(uint32_t msg, uint64_t wParam, int64_t lParam);
PARSEC_API void* Parsec_CreateClientWindow(const char* title, int width, int height);

}
