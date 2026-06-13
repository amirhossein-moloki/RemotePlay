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

## Module Breakdown

### Host Modules
1. **Screen Capture**: Uses high-performance APIs (like Windows Desktop Duplication) to grab screen frames.
2. **Video Encoder**: Encodes frames into H.264/H.265 using hardware acceleration (NVENC, AMF, QuickSync) to minimize CPU usage and latency.
3. **Streaming Server**: Packages encoded data into UDP packets and sends them to the client.
4. **Input Receiver**: Listens for input events from the client and forwards them to the Injector.
5. **Input Injector**: Uses OS-level APIs (SendInput) or virtual drivers (ViGEmBus) to simulate keyboard, mouse, and controller inputs.

### Client Modules
1. **Receiver**: Listens for UDP packets from the host and reassembles encoded frames.
2. **Video Decoder**: Decodes H.264/H.265 bitstream into raw frames using hardware acceleration.
3. **Renderer**: Displays the decoded frames on the client's screen.
4. **Input Capture**: Hooks into the client OS to capture keyboard, mouse, and gamepad events.
5. **Input Sender**: Transmits captured events to the host with minimal delay.

## Tech Stack (Prototype)
- **Language**: Python 3.x
- **Libraries**:
  - `mss`: Fast screen capture.
  - `PyAV (av)`: FFmpeg bindings for encoding/decoding.
  - `OpenCV`: Frame rendering and basic UI.
  - `pynput`: Input capture and injection.
  - `socket`: Low-level networking.

## Tech Stack (Production Recommendation)
- **Language**: C++ or Rust.
- **Libraries**:
  - **Capture**: Windows Desktop Duplication API (DDA).
  - **Encoding**: NVIDIA Video Codec SDK (NVENC), AMD AMF, Intel QuickSync.
  - **Networking**: Custom UDP protocol with Jitter Buffer, or ENet.
  - **Rendering**: SDL2 or DirectX/Vulkan for zero-copy display.
  - **Input**: ViGEmBus for virtual controller support.

## Latency Optimization Strategies
1. **Hardware Acceleration**: Always prefer GPU encoding/decoding.
2. **UDP over TCP**: Avoid retransmission delays; drop late frames instead.
3. **Zero-Copy**: Minimize data movement between CPU and GPU memory.
4. **Intra-refresh**: Use "Infinite GOP" or periodic intra-refresh to avoid large I-frames causing network spikes.
5. **Rate Control**: Implement CBR (Constant Bit Rate) with low-latency tuning (zerolatency tune in x264).
