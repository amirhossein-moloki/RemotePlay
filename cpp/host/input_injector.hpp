#pragma once

#include <vector>
#include <cstdint>
#include "../common/protocol.hpp"

namespace Host {

class InputInjector {
public:
    InputInjector();
    ~InputInjector();

    void InjectKeyboard(const Protocol::KeyboardEvent& ev);
    void InjectMouseMove(const Protocol::MouseMoveEvent& ev);
    void InjectMouseButton(const Protocol::MouseButtonEvent& ev);
    void InjectGamepad(const Protocol::GamepadState& ev);
};

} // namespace Host
