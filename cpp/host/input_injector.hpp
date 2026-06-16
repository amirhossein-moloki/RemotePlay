#pragma once

#include <vector>
#include <cstdint>
#include <map>
#include <string>
#include "../common/protocol.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef PARSEC_LITE_ENABLE_VIGEM
#include <ViGEm/Client.h>
#endif

namespace Host {

class InputInjector {
public:
    InputInjector();
    ~InputInjector();

    void InjectKeyboard(const Protocol::KeyboardEvent& ev);
    void InjectMouseMove(const Protocol::MouseMoveEvent& ev);
    void InjectMouseButton(const Protocol::MouseButtonEvent& ev);
    void InjectMouseScroll(const Protocol::MouseScrollEvent& ev);

    void HandleGamepadStatus(const std::string& clientIp, const Protocol::GamepadStatusEvent& ev);
    void InjectGamepad(const std::string& clientIp, const Protocol::GamepadState& ev);

private:
#ifdef PARSEC_LITE_ENABLE_VIGEM
    PVIGEM_CLIENT m_vigem = nullptr;
    // Map (clientIp, clientGamepadId) -> host virtual target
    std::map<std::pair<std::string, uint8_t>, PVIGEM_TARGET> m_targets;
#endif
};

} // namespace Host
