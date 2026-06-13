#include "input_injector.hpp"
#ifdef _WIN32
#include <windows.h>
#include <iostream>

namespace Host {

InputInjector::InputInjector() {}
InputInjector::~InputInjector() {}

void InputInjector::InjectKeyboard(const Protocol::KeyboardEvent& ev) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = ev.vkCode;
    input.ki.dwFlags = ev.isPressed ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::InjectMouseMove(const Protocol::MouseMoveEvent& ev) {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dx = ev.x * (65535 / GetSystemMetrics(SM_CXSCREEN));
    input.mi.dy = ev.y * (65535 / GetSystemMetrics(SM_CYSCREEN));
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::InjectMouseButton(const Protocol::MouseButtonEvent& ev) {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    if (ev.button == 1) { // Left
        input.mi.dwFlags = ev.isPressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    } else if (ev.button == 2) { // Right
        input.mi.dwFlags = ev.isPressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    }
    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::InjectGamepad(const Protocol::GamepadState& ev) {
    std::cout << "[Injector] Injecting Gamepad Input for ID: " << (int)ev.gamepadId << std::endl;
}

} // namespace Host
#endif
