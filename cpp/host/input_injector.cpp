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
    if (ev.vkCode > 255) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::InjectMouseMove(const Protocol::MouseMoveEvent& ev) {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    // Map client coordinates to absolute screen coordinates (0-65535)
    input.mi.dx = (LONG)((double)ev.x * 65536.0 / (double)ev.screenWidth);
    input.mi.dy = (LONG)((double)ev.y * 65536.0 / (double)ev.screenHeight);
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::InjectMouseButton(const Protocol::MouseButtonEvent& ev) {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    if (ev.button == 1) { // Left
        input.mi.dwFlags = ev.isPressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    } else if (ev.button == 2) { // Right
        input.mi.dwFlags = ev.isPressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    } else if (ev.button == 3) { // Middle
        input.mi.dwFlags = ev.isPressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    }
    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::InjectGamepad(const Protocol::GamepadState& ev) {
    // Detailed integration path for ViGEmBus:
    // 1. PVIGEM_CLIENT client = vigem_alloc();
    // 2. vigem_connect(client);
    // 3. PVIGEM_TARGET pad = vigem_target_x360_alloc();
    // 4. vigem_target_add(client, pad);
    // 5. XUSB_REPORT report = {0};
    //    report.wButtons = ev.buttons;
    //    report.bLeftTrigger = ev.leftTrigger;
    //    report.bRightTrigger = ev.rightTrigger;
    //    report.sThumbLX = ev.thumbLX;
    //    ...
    // 6. vigem_target_x360_update(client, pad, report);

    std::cout << "[Injector] Gamepad ID " << (int)ev.gamepadId << " - Buttons: " << ev.buttons << std::endl;
}

} // namespace Host
#endif
