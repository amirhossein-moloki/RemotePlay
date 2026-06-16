#pragma once

#include <cstdint>
#include <vector>

namespace Protocol {

// Packet types
enum class PacketType : uint8_t {
    Video = 0x01,
    Input = 0x02,
    Discovery = 0x03,
    Handshake = 0x04,
    FEC = 0x05,
    Feedback = 0x06,
    HandshakeResponse = 0x07
};

// Input subtypes
enum class InputType : uint8_t {
    Keyboard = 0x01,
    MouseMove = 0x02,
    MouseButton = 0x03,
    MouseScroll = 0x04,
    Gamepad = 0x05,
    GamepadStatus = 0x06
};

// Maximum UDP payload size (to stay within MTU)
const uint16_t MAX_UDP_PAYLOAD = 1300;

#pragma pack(push, 1)

// Video Packet Header
struct VideoHeader {
    uint8_t type;            // PacketType::Video
    uint32_t frameId;        // Frame sequence number
    uint16_t fragmentIndex;  // Fragment index in current frame
    uint16_t totalFragments; // Total fragments in current frame
    uint32_t packetSequence; // Global UDP packet sequence number
    uint64_t captureTimestamp;   // Precise capture timestamp (microseconds)
    uint64_t encodeStartTimestamp; // Encode start timestamp (microseconds)
    uint64_t encodeEndTimestamp;   // Encode end timestamp (microseconds)
    uint8_t flags;           // Bit 0: Keyframe, Bit 1-7: Reserved
    uint16_t dataSize;       // Size of the following payload
};

// FEC Packet Header (XOR based)
struct FECHeader {
    uint8_t type;            // PacketType::FEC
    uint32_t frameId;        // Frame this FEC belongs to
    uint16_t fragmentStart;  // First fragment index covered by this FEC
    uint16_t fragmentCount;  // Number of fragments covered
    uint32_t packetSequence; // Global UDP packet sequence number
    uint16_t dataSize;       // Size of the XORed payload
};

// Feedback Packet Header (Client -> Host)
struct FeedbackHeader {
    uint8_t type;            // PacketType::Feedback
    uint32_t lastReceivedFrameId;
    float lossRate;
    uint32_t rttMs;
};

// Input Packet Header
struct InputHeader {
    uint8_t type;            // PacketType::Input
    uint8_t inputType;       // InputType subtype
    uint64_t timestamp;      // Client-side capture timestamp (microseconds)
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
    uint32_t screenWidth;
    uint32_t screenHeight;
    uint8_t isRelative;
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

// Gamepad State (XInput-compatible)
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

// Gamepad Status Event (Connect/Disconnect)
struct GamepadStatusEvent {
    uint8_t gamepadId;
    uint8_t isConnected;
};

// Handshake Packet (Client -> Host)
struct HandshakePacket {
    uint8_t type;            // PacketType::Handshake
    char username[32];
};

// Handshake Response Packet (Host -> Client)
struct HandshakeResponsePacket {
    uint8_t type;            // PacketType::HandshakeResponse
    uint8_t approved;        // 1: Approved, 0: Rejected
};

#pragma pack(pop)

} // namespace Protocol
