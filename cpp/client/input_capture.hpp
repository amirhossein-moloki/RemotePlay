#pragma once

#ifdef _WIN32
#include <windows.h>
#include <xinput.h>
#endif
#include <vector>
#include <cstdint>
#include <functional>
#include <chrono>
#include <cstring>
#include "../common/protocol.hpp"

namespace Client {

class InputCapture {
public:
    using InputCallback = std::function<void(const uint8_t*, size_t)>;

    InputCapture(InputCallback callback);

#ifdef _WIN32
    bool RegisterDevices(HWND hwnd);
    void HandleRawInput(LPARAM lParam);
#endif
    void PollGamepads();

private:
#ifdef _WIN32
    XINPUT_STATE m_lastGamepadState[XUSER_MAX_COUNT] = { 0 };
    bool m_gamepadConnected[XUSER_MAX_COUNT] = { false };
    HWND m_hwnd = nullptr;

    struct {
        int32_t relX = 0;
        int32_t relY = 0;
        uint64_t lastSentUs = 0;
        bool hasPending = false;
    } m_mouseBatch;
#endif

    template<typename T>
    void SendPacket(const Protocol::InputHeader& header, const T& payload) {
        Protocol::InputHeader h = header;
        h.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        uint8_t buffer[sizeof(Protocol::InputHeader) + sizeof(T)];
        std::memcpy(buffer, &h, sizeof(h));
        std::memcpy(buffer + sizeof(h), &payload, sizeof(T));
        m_callback(buffer, sizeof(buffer));
    }

    InputCallback m_callback;
};

} // namespace Client
