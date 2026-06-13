#pragma once

#ifdef _WIN32
#include <windows.h>
#endif
#include <vector>
#include <cstdint>
#include <functional>
#include "../common/protocol.hpp"

namespace Client {

class InputCapture {
public:
    using InputCallback = std::function<void(const std::vector<uint8_t>&)>;

    InputCapture(InputCallback callback);

#ifdef _WIN32
    bool RegisterDevices(HWND hwnd);
    void HandleRawInput(LPARAM lParam);
#endif
    void PollGamepads();

private:
    template<typename T>
    void SendPacket(const Protocol::InputHeader& header, const T& payload);

    InputCallback m_callback;
};

} // namespace Client
