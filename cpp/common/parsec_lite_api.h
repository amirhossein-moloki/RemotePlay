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
    char username[32];
    int bitrate;
    int fps;
    int targetWidth;
    int targetHeight;
    float resolutionScale;
    bool isHost;
    bool runBoth;
    bool useHardwareEncoding;
    bool autoApprove;
    int encoderPreset; // 0: Performance, 1: Balanced, 2: Quality
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

enum class ParsecError {
    SUCCESS = 0,
    NETWORK_BIND_FAILED,
    HARDWARE_INIT_FAILED,
    HANDSHAKE_TIMEOUT,
    HANDSHAKE_REJECTED,
    CONNECTION_LOST,
    UNEXPECTED_ERROR,
    DECODER_INIT_FAILED,
    ENCODER_INIT_FAILED,
    RENDERER_INIT_FAILED,
    QSV_INIT_FAILED,
    D3D11_DEVICE_LOST
};

typedef void (*ParsecConnectionCallback)(const char* username, const char* ip, uint16_t port);
typedef void (*ParsecErrorCallback)(ParsecError error, const char* message);

PARSEC_API bool Parsec_Initialize();
PARSEC_API void Parsec_StartSession(ParsecConfig config);
PARSEC_API void Parsec_StopSession();
PARSEC_API bool Parsec_GetTelemetry(ParsecTelemetry* outTelemetry);
PARSEC_API void Parsec_Shutdown();

PARSEC_API void Parsec_SetConnectionCallback(ParsecConnectionCallback callback);
PARSEC_API void Parsec_SetErrorCallback(ParsecErrorCallback callback);
PARSEC_API void Parsec_ApproveConnection(const char* ip, uint16_t port, bool approved);

// UI Support
PARSEC_API void Parsec_HandleMessage(uint32_t msg, uint64_t wParam, int64_t lParam);
PARSEC_API void* Parsec_CreateClientWindow(const char* title, int width, int height);

}
