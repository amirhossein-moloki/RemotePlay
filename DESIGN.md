# LAN Game Streaming System Design (Parsec-lite C++)

## 1. High-Level Architecture

The system is designed as a single Windows executable that can operate in either **Host** or **Client** mode. It enables low-latency desktop/application streaming over a LAN with input back-channeling.

```text
+-----------------------------------------------------------------------+
|                         Unified Executable (.exe)                     |
|                                                                       |
|      +---------------------------------------------------------+      |
|      |                  UI / Mode Selection (ImGui)            |      |
|      +---------------------------------------------------------+      |
|             |                                     |                   |
|      [ HOST MODE ]                         [ CLIENT MODE ]            |
|             |                                     |                   |
|  +-----------------------+           +-----------------------------+  |
|  | Capture (DXGI DDA)    |           | Receiver (UDP + Jitter Buf) |  |
|  +-----------------------+           +-----------------------------+  |
|  | Encoder (NVENC/AMF)   |           | Decoder (DXVA2/D3D11VA)     |  |
|  +-----------------------+           +-----------------------------+  |
|  | Streamer (UDP)        |<---Vid----| Renderer (SDL2/D3D11)       |  |
|  +-----------------------+           +-----------------------------+  |
|  | Injector (ViGEm/Input)|<---Inp----| Input Capture (RawInput)    |  |
|  +-----------------------+           +-----------------------------+  |
|             |                                     |                   |
+-------------|-------------------------------------|-------------------+
              |           [ LAN NETWORK ]           |
              +-------------------------------------+
```

## 2. Module Breakdown

### 2.1. Common Modules
- **NetworkManager**: Handles network interface enumeration, explicit IP binding, and UDP socket abstraction.
- **SessionManager**: Manages the discovery (UDP Broadcast), handshake, and capability negotiation.
- **Protocol**: Defines the binary structures for video fragments and input events.

### 2.2. Host Modules
- **CaptureModule (DXGI)**: Uses Desktop Duplication API to capture desktop frames directly into GPU textures.
- **EncoderModule**: Abstraction layer for hardware encoders (NVENC, AMF, QuickSync). Performs zero-copy encoding from DXGI textures.
- **StreamingServer**: Packetizes encoded bitstreams and broadcasts them to connected clients.
- **InputInjectorHost**: Injects keyboard/mouse events via `SendInput` and gamepad events via `ViGEmBus`.

### 2.3. Client Modules
- **ReceiverModule**: Reassembles UDP fragments into full frames. Implements a minimal jitter buffer to handle network variance.
- **DecoderModule**: Leverages hardware decoders (e.g., via FFmpeg or Media Foundation) to decode H.264 bitstream.
- **InputCaptureClient**: Captures local keyboard, mouse, and XInput/DirectInput gamepad events.
- **UIModule**: Handles startup mode selection and network interface selection.

## 3. Recommended C++ Stack
- **Framework**: Standard C++17/20.
- **Networking**: Winsock2 (Direct Socket API) for maximum control.
- **Graphics/Capture**: DXGI (Desktop Duplication), D3D11 for texture management.
- **Encoding/Decoding**: FFmpeg (libavcodec) with hardware acceleration or vendor SDKs directly (NVENC/AMF).
- **UI**: Dear ImGui for a lightweight, overlay-capable interface.
- **Input Injection**: ViGEmClient SDK.
- **Input Capture**: Windows Raw Input API or SDL2.

## 4. UDP Protocol Design (Binary Optimized)

### 4.1. Video Packet (Type 0x01)
| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | Type | 0x01 for Video |
| 1 | 4 | SeqID | Frame sequence number |
| 5 | 2 | FragIdx | Fragment index in current frame |
| 7 | 2 | TotalFrags | Total fragments in current frame |
| 9 | 8 | Timestamp | Capture timestamp (microseconds) |
| 17 | 1 | Flags | Bit 0: Keyframe, Bit 1-7: Reserved |
| 18 | 2 | DataSize | Size of the following payload |
| 20 | N | Payload | Encoded H.264 data fragment |

### 4.2. Input Packet (Type 0x02)
| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | Type | 0x02 for Input |
| 1 | 1 | SubType | 1: KB, 2: Mouse, 3: Gamepad |
| 2 | N | Payload | Serialized event (e.g., VK code, Mouse X/Y, Gamepad state) |

## 5. Implementation Roadmap

### Phase 1: MVP (Minimum Viable Product)
- Unified CLI-based executable.
- Winsock2 UDP transport for raw frames (no encoding yet).
- Basic `SendInput` keyboard forwarding.
- Manual IP entry for connection.

### Phase 2: Performance Optimization
- DXGI Desktop Duplication integration.
- Hardware-accelerated H.264 encoding (NVENC).
- UDP packet fragmentation and reassembly.
- ViGEmBus integration for virtual controllers.

### Phase 3: Advanced Features
- ImGui-based GUI for interface selection and mode switching.
- Automatic Host discovery via UDP broadcast.
- Adaptive bitrate based on RTT.
- Periodic Intra-Refresh for better error resilience.
