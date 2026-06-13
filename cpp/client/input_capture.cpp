#include "input_capture.hpp"
#ifdef _WIN32
#include <xinput.h>
#include <iostream>

#pragma comment(lib, "xinput.lib")

namespace Client {

InputCapture::InputCapture(InputCallback callback) : m_callback(callback) {}

bool InputCapture::RegisterDevices(HWND hwnd) {
    RAWINPUTDEVICE rid[2];

    // Mouse
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x02;
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = hwnd;

    // Keyboard
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage = 0x06;
    rid[1].dwFlags = RIDEV_INPUTSINK;
    rid[1].hwndTarget = hwnd;

    return RegisterRawInputDevices(rid, 2, sizeof(rid[0]));
}

void InputCapture::HandleRawInput(LPARAM lParam) {
    UINT dwSize;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
    std::vector<uint8_t> buffer(dwSize);
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer.data(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) return;

    RAWINPUT* raw = (RAWINPUT*)buffer.data();
    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        Protocol::InputHeader header = { (uint8_t)Protocol::PacketType::Input, (uint8_t)Protocol::InputType::Keyboard };
        Protocol::KeyboardEvent kb = { raw->data.keyboard.VKey, (uint8_t)!(raw->data.keyboard.Flags & RI_KEY_BREAK) };
        SendPacket(header, kb);
    } else if (raw->header.dwType == RIM_TYPEMOUSE) {
        if (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
            Protocol::InputHeader header = { (uint8_t)Protocol::PacketType::Input, (uint8_t)Protocol::InputType::MouseMove };
            Protocol::MouseMoveEvent mm = { raw->data.mouse.lLastX, raw->data.mouse.lLastY };
            SendPacket(header, mm);
        }
    }
}

void InputCapture::PollGamepads() {
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            Protocol::InputHeader header;
            header.type = (uint8_t)Protocol::PacketType::Input;
            header.inputType = (uint8_t)Protocol::InputType::Gamepad;
            header.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

            Protocol::GamepadState gp;
            gp.gamepadId = (uint8_t)i;
            gp.buttons = state.Gamepad.wButtons;
            gp.leftTrigger = state.Gamepad.bLeftTrigger;
            gp.rightTrigger = state.Gamepad.bRightTrigger;
            gp.thumbLX = state.Gamepad.sThumbLX;
            gp.thumbLY = state.Gamepad.sThumbLY;
            gp.thumbRX = state.Gamepad.sThumbRX;
            gp.thumbRY = state.Gamepad.sThumbRY;

            std::vector<uint8_t> packet(sizeof(header) + sizeof(gp));
            memcpy(packet.data(), &header, sizeof(header));
            memcpy(packet.data() + sizeof(header), &gp, sizeof(gp));
            m_callback(packet);
        }
    }
}

template<typename T>
void InputCapture::SendPacket(const Protocol::InputHeader& header, const T& payload) {
    Protocol::InputHeader h = header;
    h.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    std::vector<uint8_t> packet(sizeof(h) + sizeof(T));
    memcpy(packet.data(), &h, sizeof(h));
    memcpy(packet.data() + sizeof(h), &payload, sizeof(T));
    m_callback(packet);
}

} // namespace Client
#else
namespace Client {
InputCapture::InputCapture(InputCallback callback) : m_callback(callback) {}
void InputCapture::PollGamepads() {}
}
#endif
