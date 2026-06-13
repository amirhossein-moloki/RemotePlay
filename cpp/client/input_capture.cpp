#include <windows.h>
#include <xinput.h>
#include <iostream>
#include <functional>
#include "../common/protocol.hpp"

#pragma comment(lib, "xinput.lib")

namespace Client {

class InputCapture {
public:
    using InputCallback = std::function<void(const std::vector<uint8_t>&)>;

    InputCapture(InputCallback callback) : m_callback(callback) {}

    // Register for Raw Input (Keyboard and Mouse)
    bool RegisterDevices(HWND hwnd) {
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

    void HandleRawInput(LPARAM lParam) {
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

    void PollGamepads() {
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
            XINPUT_STATE state;
            if (XInputGetState(i, &state) == ERROR_SUCCESS) {
                // Check if state changed significantly to avoid spam
                Protocol::InputHeader header;
                header.type = (uint8_t)Protocol::PacketType::Input;
                header.inputType = (uint8_t)Protocol::InputType::Gamepad;

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

private:
    template<typename T>
    void SendPacket(const Protocol::InputHeader& header, const T& payload) {
        std::vector<uint8_t> packet(sizeof(header) + sizeof(T));
        memcpy(packet.data(), &header, sizeof(header));
        memcpy(packet.data() + sizeof(header), &payload, sizeof(T));
        m_callback(packet);
    }

    InputCallback m_callback;
};

} // namespace Client
