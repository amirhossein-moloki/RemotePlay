#include "input_injector.hpp"
#ifdef _WIN32
#include <windows.h>
#include <iostream>
#include <chrono>

namespace Host {

InputInjector::InputInjector() {
#ifdef _WIN32
    m_vigem = vigem_alloc();
    if (m_vigem) {
        VIGEM_ERROR err = vigem_connect(m_vigem);
        if (!VIGEM_SUCCESS(err)) {
            std::cerr << "[Injector] ViGEmBus connection failed: " << err << std::endl;
            vigem_free(m_vigem);
            m_vigem = nullptr;
        }
    }
#endif
}

InputInjector::~InputInjector() {
#ifdef _WIN32
    if (m_vigem) {
        for (auto& pair : m_targets) {
            vigem_target_remove(m_vigem, pair.second);
            vigem_target_free(pair.second);
        }
        vigem_disconnect(m_vigem);
        vigem_free(m_vigem);
    }
#endif
}

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

void InputInjector::HandleGamepadStatus(const std::string& clientIp, const Protocol::GamepadStatusEvent& ev) {
#ifdef _WIN32
    if (!m_vigem) return;

    auto key = std::make_pair(clientIp, ev.gamepadId);
    if (ev.isConnected) {
        if (m_targets.find(key) == m_targets.end()) {
            PVIGEM_TARGET target = vigem_target_x360_alloc();
            VIGEM_ERROR err = vigem_target_add(m_vigem, target);
            if (VIGEM_SUCCESS(err)) {
                m_targets[key] = target;
                std::cout << "[Injector] Plugged in virtual X360 controller for " << clientIp << " ID " << (int)ev.gamepadId << std::endl;
            } else {
                std::cerr << "[Injector] Failed to plug in virtual controller: " << err << std::endl;
                vigem_target_free(target);
            }
        }
    } else {
        auto it = m_targets.find(key);
        if (it != m_targets.end()) {
            vigem_target_remove(m_vigem, it->second);
            vigem_target_free(it->second);
            m_targets.erase(it);
            std::cout << "[Injector] Unplugged virtual X360 controller for " << clientIp << " ID " << (int)ev.gamepadId << std::endl;
        }
    }
#endif
}

void InputInjector::InjectGamepad(const std::string& clientIp, const Protocol::GamepadState& ev) {
#ifdef _WIN32
    if (!m_vigem) return;

    auto it = m_targets.find(std::make_pair(clientIp, ev.gamepadId));
    if (it != m_targets.end()) {
        XUSB_REPORT report;
        XUSB_REPORT_INIT(&report);
        report.wButtons = ev.buttons;
        report.bLeftTrigger = ev.leftTrigger;
        report.bRightTrigger = ev.rightTrigger;
        report.sThumbLX = ev.thumbLX;
        report.sThumbLY = ev.thumbLY;
        report.sThumbRX = ev.thumbRX;
        report.sThumbRY = ev.thumbRY;

        vigem_target_x360_update(m_vigem, it->second, report);
    }
#endif
}

} // namespace Host
#endif
