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
    HandshakeResponse = 0x07,
    HandshakeSecure = 0x08,
    HandshakeSecureResponse = 0x09,
    Secure = 0x0A,
    Audio = 0x0B,
    TimeSync = 0x0C,
    CandidateDiscovery = 0x0D,
    RelayData = 0x0E,
    RetransmitRequest = 0x0F
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

// Forward declarations
struct SecureHeader;
struct HandshakeSecurePacket;
struct HandshakeSecureResponsePacket;

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
    uint8_t flags;           // Bit 0: Keyframe, Bit 1: HEVC, Bit 2-7: Reserved
    uint16_t dataSize;       // Size of the following payload
};

// Audio Packet Header
struct AudioHeader {
    uint8_t type;            // PacketType::Audio
    uint32_t frameId;        // Audio frame sequence
    uint64_t timestamp;      // Global clock timestamp (microseconds)
    uint16_t dataSize;       // Size of the Opus payload
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
    float avgDecodeTimeMs;
    uint8_t requestKeyframe; // 1 to request an IDR frame, 0 otherwise
    uint16_t targetBitrateKbps; // ABR: Client-side suggested bitrate
    float resolutionScale;      // ABR: Client-side suggested resolution scale
};

// Input Packet Header
struct InputHeader {
    uint8_t type;            // PacketType::Input
    uint8_t inputType;       // InputType subtype
    uint64_t timestamp;      // Client-side capture timestamp (microseconds)
};

// Time Sync Packet (Client -> Host)
struct TimeSyncPacket {
    uint8_t type;            // PacketType::TimeSync
    uint64_t clientSendTimestamp;
};

// Time Sync Response (Host -> Client)
struct TimeSyncResponsePacket {
    uint8_t type;            // PacketType::TimeSync
    uint64_t clientSendTimestamp;
    uint64_t hostReceiveTimestamp;
    uint64_t hostSendTimestamp;
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

// Secure Handshake Packet (Client -> Host)
struct HandshakeSecurePacket {
    uint8_t type;            // PacketType::HandshakeSecure
    uint8_t clientPublicKey[32];
    char username[32];
};

// Secure Handshake Response Packet (Host -> Client)
struct HandshakeSecureResponsePacket {
    uint8_t type;            // PacketType::HandshakeSecureResponse
    uint8_t hostPublicKey[32];
    uint8_t approved;        // 1: Approved, 0: Rejected
    uint64_t sessionId;
};

// Secure Envelope Header
struct SecureHeader {
    uint8_t type;            // PacketType::Secure
    uint64_t sessionId;
    uint64_t sequenceNumber;
    uint16_t encryptedSize;
    uint8_t authTag[16];     // Poly1305 MAC
};

// Handshake Packet (Client -> Host) - DEPRECATED for WAN
struct HandshakePacket {
    uint8_t type;            // PacketType::Handshake
    char username[32];
};

// Handshake Response Packet (Host -> Client) - DEPRECATED for WAN
struct HandshakeResponsePacket {
    uint8_t type;            // PacketType::HandshakeResponse
    uint8_t approved;        // 1: Approved, 0: Rejected
};

// Candidate Discovery Packet (Peers exchange Srflx and Host candidates)
struct CandidatePacket {
    uint8_t type;            // PacketType::CandidateDiscovery
    char ip[64];             // Discovered IP (reflexive or host)
    uint16_t port;           // Discovered Port
    uint8_t priority;        // Lower is better (0: Local, 1: Srflx, 2: Relay)
};

// Relay Data Header (Encapsulation for edge nodes)
struct RelayHeader {
    uint8_t type;            // PacketType::RelayData
    uint64_t sessionId;      // Target session ID
    uint8_t targetUUID[16];  // Edge routing destination
    uint16_t dataSize;       // Encapsulated payload size
};

// Selective Retransmission Request (NACK)
struct RetransmitRequestPacket {
    uint8_t type;            // PacketType::RetransmitRequest
    uint32_t frameId;
    uint16_t fragmentIndex;
};

#pragma pack(pop)

} // namespace Protocol
