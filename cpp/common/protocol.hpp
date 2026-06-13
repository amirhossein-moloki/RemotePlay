#pragma once

#include <cstdint>
#include <vector>

namespace Protocol {

// Packet types
enum class PacketType : uint8_t {
    Video = 0x01,
    Input = 0x02,
    Discovery = 0x03,
    Handshake = 0x04
};

// Input subtypes
enum class InputType : uint8_t {
    Keyboard = 0x01,
    MouseMove = 0x02,
    MouseButton = 0x03,
    MouseScroll = 0x04,
    Gamepad = 0x05
};

// Maximum UDP payload size (to stay within MTU)
const uint16_t MAX_UDP_PAYLOAD = 1400;

#pragma pack(push, 1)

// Video Packet Header
struct VideoHeader {
    uint8_t type;            // PacketType::Video
    uint32_t sequence;       // Frame sequence number
    uint16_t fragmentIndex;  // Fragment index in current frame
    uint16_t totalFragments; // Total fragments in current frame
    uint64_t timestamp;      // Capture timestamp (microseconds)
    uint8_t flags;           // Bit 0: Keyframe, Bit 1-7: Reserved
    uint16_t dataSize;       // Size of the following payload
};

// Input Packet Header
struct InputHeader {
    uint8_t type;            // PacketType::Input
    uint8_t inputType;       // InputType subtype
};

// Keyboard Event
struct KeyboardEvent {
    uint16_t vkCode;
    uint8_t isPressed;
};

// Mouse Move Event
struct MouseMoveEvent {
    int32_t x;
    int32_t y;
};

// Mouse Button Event
struct MouseButtonEvent {
    uint8_t button; // 1: Left, 2: Right, 3: Middle
    uint8_t isPressed;
};

// Mouse Scroll Event
struct MouseScrollEvent {
    int32_t delta;
};

// Gamepad State (XInput-like)
struct GamepadState {
    uint8_t gamepadId;
    uint16_t buttons;
    uint8_t leftTrigger;
    uint8_t rightTrigger;
    int16_t thumbLX;
    int16_t thumbLY;
    int16_t thumbRX;
    int16_t thumbRY;
};

#pragma pack(pop)

} // namespace Protocol
