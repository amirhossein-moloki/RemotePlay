#include "input_capture.hpp"
#ifdef _WIN32
#include <xinput.h>
#include <iostream>

#pragma comment(lib, "xinput.lib")

namespace Client {

InputCapture::InputCapture(InputCallback callback) : m_callback(callback) {}

bool InputCapture::RegisterDevices(HWND hwnd) {
    m_hwnd = hwnd;
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
        // Heuristic: If the cursor is visible, we prefer absolute coordinates for perfect desktop sync.
        // If hidden (e.g. in an FPS game), we use relative deltas to avoid breaking camera controls.
        CURSORINFO ci = { sizeof(CURSORINFO) };
        bool cursorVisible = true;
        if (GetCursorInfo(&ci)) {
            cursorVisible = (ci.flags & CURSOR_SHOWING);
        }

        bool isMovement = (raw->data.mouse.lLastX != 0 || raw->data.mouse.lLastY != 0);
        bool isAbsoluteRaw = (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE);

        // Always handle movement if raw input says it's absolute, OR if it's relative movement
        if (isMovement || isAbsoluteRaw || cursorVisible) {
            Protocol::InputHeader header = { (uint8_t)Protocol::PacketType::Input, (uint8_t)Protocol::InputType::MouseMove };
            Protocol::MouseMoveEvent mm = { 0 };

            if (cursorVisible && GetFocus() == m_hwnd) {
                // Desktop mode: Send absolute coordinates derived from client cursor position
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(m_hwnd, &pt);
                RECT rect;
                GetClientRect(m_hwnd, &rect);

                mm.isRelative = 0;
                mm.x = pt.x;
                mm.y = pt.y;
                mm.screenWidth = rect.right - rect.left;
                mm.screenHeight = rect.bottom - rect.top;

                // Only send if coordinates are within the client area to prevent "jumping" to 0,0 when clicking title bar
                if (mm.x >= 0 && mm.y >= 0 && mm.x <= (int32_t)mm.screenWidth && mm.y <= (int32_t)mm.screenHeight) {
                    SendPacket(header, mm);
                }
            } else if (isMovement) {
                // Game mode or cursor hidden: Use raw relative/absolute data
                mm.isRelative = !isAbsoluteRaw;
                mm.x = raw->data.mouse.lLastX;
                mm.y = raw->data.mouse.lLastY;

                if (mm.isRelative) {
                    // BATCHING: Accumulate relative movement
                    m_mouseBatch.relX += mm.x;
                    m_mouseBatch.relY += mm.y;
                    m_mouseBatch.hasPending = true;

                    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch()).count();

                    // Send if 4ms elapsed (250Hz max rate for mouse deltas)
                    if (now - m_mouseBatch.lastSentUs >= 4000) {
                        mm.x = m_mouseBatch.relX;
                        mm.y = m_mouseBatch.relY;
                        SendPacket(header, mm);
                        m_mouseBatch.relX = 0;
                        m_mouseBatch.relY = 0;
                        m_mouseBatch.lastSentUs = now;
                        m_mouseBatch.hasPending = false;
                    }
                } else {
                    RECT rect;
                    GetClientRect(m_hwnd, &rect);
                    mm.screenWidth = rect.right - rect.left;
                    mm.screenHeight = rect.bottom - rect.top;
                    SendPacket(header, mm);
                }
            }
        }

        // Handle Buttons
        USHORT flags = raw->data.mouse.usButtonFlags;
        auto sendBtn = [&](uint8_t btn, bool pressed) {
            Protocol::InputHeader h = { (uint8_t)Protocol::PacketType::Input, (uint8_t)Protocol::InputType::MouseButton };
            Protocol::MouseButtonEvent be = { btn, (uint8_t)(pressed ? 1 : 0) };
            SendPacket(h, be);
        };

        if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) sendBtn(1, true);
        if (flags & RI_MOUSE_LEFT_BUTTON_UP) sendBtn(1, false);
        if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) sendBtn(2, true);
        if (flags & RI_MOUSE_RIGHT_BUTTON_UP) sendBtn(2, false);
        if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) sendBtn(3, true);
        if (flags & RI_MOUSE_MIDDLE_BUTTON_UP) sendBtn(3, false);
        if (flags & RI_MOUSE_BUTTON_4_DOWN) sendBtn(4, true);
        if (flags & RI_MOUSE_BUTTON_4_UP) sendBtn(4, false);
        if (flags & RI_MOUSE_BUTTON_5_DOWN) sendBtn(5, true);
        if (flags & RI_MOUSE_BUTTON_5_UP) sendBtn(5, false);

        // Handle Wheel
        if (flags & RI_MOUSE_WHEEL) {
            Protocol::InputHeader h = { (uint8_t)Protocol::PacketType::Input, (uint8_t)Protocol::InputType::MouseScroll };
            Protocol::MouseScrollEvent se = { (int32_t)(SHORT)raw->data.mouse.usButtonData };
            SendPacket(h, se);
        }
    }
}

void InputCapture::PollGamepads() {
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state;
        DWORD dwResult = XInputGetState(i, &state);
        bool connected = (dwResult == ERROR_SUCCESS);

        if (connected != m_gamepadConnected[i]) {
            m_gamepadConnected[i] = connected;
            Protocol::InputHeader header = { (uint8_t)Protocol::PacketType::Input, (uint8_t)Protocol::InputType::GamepadStatus };
            Protocol::GamepadStatusEvent status = { (uint8_t)i, (uint8_t)(connected ? 1 : 0) };
            SendPacket(header, status);

            if (connected) {
                // Initialize last state to force an update on first connection
                m_lastGamepadState[i] = state;
                m_lastGamepadState[i].dwPacketNumber--; // Ensure it's different
            }
        }

        if (connected) {
            // Only send update if state changed (delta-compression)
            if (state.dwPacketNumber != m_lastGamepadState[i].dwPacketNumber) {
                m_lastGamepadState[i] = state;

                Protocol::InputHeader header = { (uint8_t)Protocol::PacketType::Input, (uint8_t)Protocol::InputType::Gamepad };
                Protocol::GamepadState gp;
                gp.gamepadId = (uint8_t)i;
                gp.buttons = state.Gamepad.wButtons;
                gp.leftTrigger = state.Gamepad.bLeftTrigger;
                gp.rightTrigger = state.Gamepad.bRightTrigger;
                gp.thumbLX = state.Gamepad.sThumbLX;
                gp.thumbLY = state.Gamepad.sThumbLY;
                gp.thumbRX = state.Gamepad.sThumbRX;
                gp.thumbRY = state.Gamepad.sThumbRY;

                SendPacket(header, gp);
            }
        }
    }
}


} // namespace Client
#else
namespace Client {
InputCapture::InputCapture(InputCallback callback) : m_callback(callback) {}
void InputCapture::PollGamepads() {}
}
#endif
