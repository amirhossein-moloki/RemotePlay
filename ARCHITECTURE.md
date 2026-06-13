# LAN Game Streaming System Architecture (Parsec-lite)

## Overview
This system allows for low-latency game streaming over a Local Area Network (LAN). It consists of a Host (the machine running the game) and one or more Clients (the machines receiving the stream and sending inputs).

## Architecture Diagram

```text
+---------------------+                       +---------------------+
|       HOST          |                       |       CLIENT        |
|                     |                       |                     |
|  +---------------+  |       Video (UDP)     |  +---------------+  |
|  |  Screen Cap   |-------------------------->|    Receiver   |  |
|  +---------------+  |                       |  +---------------+  |
|         |           |                       |         |           |
|  +---------------+  |                       |  +---------------+  |
|  |    Encoder    |  |                       |  |    Decoder    |  |
|  +---------------+  |                       |  +---------------+  |
|         |           |                       |         |           |
|  +---------------+  |                       |  +---------------+  |
|  |   Streamer    |  |                       |  |    Renderer   |  |
|  +---------------+  |                       |  +---------------+  |
|                     |                       |                     |
|  +---------------+  |       Input (UDP)     |  +---------------+  |
|  | Input Injector |<--------------------------| Input Capture |  |
|  +---------------+  |                       |  +---------------+  |
|                     |                       |                     |
+---------------------+                       +---------------------+
```

## Detailed Module Breakdown

### Host Pipeline
1.  **Capture Module**: Responsible for grabbing the desktop or application window frames.
    -   *Prototype*: Uses `mss` for cross-platform capture.
    -   *Production*: Uses Windows Desktop Duplication API (DDA) for zero-copy GPU capture.
2.  **Encoder Module**: Compresses raw frames into H.264/H.265 bitstream.
    -   *Prototype*: PyAV (FFmpeg) with `ultrafast` preset and `zerolatency` tune.
    -   *Production*: Direct integration with NVIDIA NVENC SDK, AMD AMF, or Intel QuickSync for hardware-accelerated encoding.
3.  **Streaming Server**: Manages network connections and packetizes encoded data.
    -   *Function*: Handles fragmentation, sequence numbering, and UDP transmission.

### Client Pipeline
1.  **Receiver**: Reassembles UDP packets into complete frames.
    -   *Function*: Handles out-of-order packets and packet loss via a jitter buffer.
2.  **Decoder Module**: Decodes the bitstream back into raw image frames.
    -   *Prototype*: PyAV (FFmpeg) software or hardware decoding.
    -   *Production*: Hardware-accelerated decoding (e.g., via DXVA2 or NVDEC).
3.  **Renderer**: Displays frames to the user.
    -   *Prototype*: OpenCV `imshow`.
    -   *Production*: SDL2, DirectX, or Vulkan for low-latency rendering.

### Input Management
1.  **Input Capture (Client)**: Monitors keyboard, mouse, and gamepad events.
    -   *Prototype*: `pynput` and `pygame`.
2.  **Input Injector (Host)**: Simulates inputs on the host machine.
    -   *Prototype*: `pynput`.
    -   *Production*: `SendInput` API or ViGEmBus driver for virtual controller support.

## Technology Stack Justification

| Layer | Recommended (Production) | Reason |
| :--- | :--- | :--- |
| **Language** | C++ or Rust | Performance, low-level memory control, and access to native SDKs. |
| **Capture** | Desktop Duplication API | High-performance, low-latency screen capture on Windows. |
| **Video Codec** | H.264 / H.265 (HEVC) | Broad hardware support and excellent compression-to-latency ratio. |
| **Networking** | Custom UDP or ENet | UDP is essential to avoid TCP's head-of-line blocking. |
| **Input Injection** | ViGEmBus | Allows for full virtual Xbox/DualShock controller emulation. |

## Network Flow
1.  **Handshake**: Client sends a `CONNECT` broadcast or direct message. Host acknowledges.
2.  **Stream Negotiation**: Host and Client agree on resolution, FPS, and bitrate.
3.  **Data Transmission**:
    -   **Video**: Host -> Client (Unreliable/UDP).
    -   **Input**: Client -> Host (Reliable-ordered UDP or low-latency TCP).
