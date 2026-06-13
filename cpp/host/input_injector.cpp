#include <windows.h>
#include <iostream>
#include "../common/protocol.hpp"

// Note: ViGEmBus requires ViGEmClient.h and linking against ViGEmClient.lib
// This implementation assumes the environment has it set up as described in ARCHITECTURE.md.

namespace Host {

class InputInjector {
public:
    InputInjector() {}
    ~InputInjector() {}

    void InjectKeyboard(const Protocol::KeyboardEvent& ev) {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = ev.vkCode;
        input.ki.dwFlags = ev.isPressed ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }

    void InjectMouseMove(const Protocol::MouseMoveEvent& ev) {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dx = ev.x * (65535 / GetSystemMetrics(SM_CXSCREEN));
        input.mi.dy = ev.y * (65535 / GetSystemMetrics(SM_CYSCREEN));
        input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(INPUT));
    }

    void InjectMouseButton(const Protocol::MouseButtonEvent& ev) {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        if (ev.button == 1) { // Left
            input.mi.dwFlags = ev.isPressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        } else if (ev.button == 2) { // Right
            input.mi.dwFlags = ev.isPressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        }
        SendInput(1, &input, sizeof(INPUT));
    }

    // ViGEm integration placeholder
    void InjectGamepad(const Protocol::GamepadState& ev) {
        /**
         * PRODUCTION NOTE:
         * To use ViGEmBus:
         *
         * PVIGEM_CLIENT client = vigem_alloc();
         * vigem_connect(client);
         * PVIGEM_TARGET pad = vigem_target_x360_alloc();
         * vigem_target_add(client, pad);
         *
         * XUSB_REPORT report;
         * XUSB_REPORT_INIT(&report);
         * report.wButtons = ev.buttons;
         * report.bLeftTrigger = ev.leftTrigger;
         * report.bRightTrigger = ev.rightTrigger;
         * report.sThumbLX = ev.thumbLX;
         * ...
         * vigem_target_x360_update(client, pad, report);
         */
        std::cout << "[Injector] Injecting Gamepad Input for ID: " << (int)ev.gamepadId << std::endl;
    }
};

} // namespace Host
