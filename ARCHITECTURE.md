# Parsec-lite System Architecture

## Overview
Parsec-lite is a high-performance, low-latency game streaming system designed for LAN environments. It follows a multi-threaded, pipelined architecture to minimize end-to-end delay.

## Architecture Diagram

```text
[ HOST SIDE ]                                     [ CLIENT SIDE ]
+---------------------+                           +---------------------+
|  Capture (DXGI DDA) |                           |  UDP Socket Receiver|
+----------|----------+                           +----------|----------+
           v                                                 v
+---------------------+                           +---------------------+
|  Encode (NVENC/AMF) |                           |  FEC Recovery &     |
+----------|----------+                           |  Reassembly         |
           v                                      +----------|----------+
+---------------------+                                      v
|  Packetizer & FEC   |                           +---------------------+
+----------|----------+                           |  Adaptive Jitter    |
           v                                      |  Buffer             |
+---------------------+                           +----------|----------+
|  UDP Sender Thread  | -------- [LAN] -------->             v
+---------------------+                           +---------------------+
           ^                                      |  Decode (D3D11VA)   |
           |                                      +----------|----------+
           |           [ Input Back-channel ]                v
+---------------------+                           +---------------------+
|  Input Injector     | <------------------------ |  Input Capture      |
|  (SendInput/ViGEm)  |                           |  (RawInput/XInput)  |
+---------------------+                           +---------------------+
```

## Multi-threaded Pipeline

### Host Pipeline
1.  **Capture Thread**: Uses Windows Desktop Duplication API (DXGI) to capture frames directly into GPU textures. Operates at the target frame rate (e.g., 60 FPS).
2.  **Encoder Thread**: Consumes captured textures and uses hardware-accelerated encoders (NVENC/AMF/QSV) via FFmpeg. Employs "Zero-Copy" to avoid CPU-GPU transfers.
3.  **Packetizer Thread**: Fragments encoded bitstreams into MTU-sized UDP packets (1300 bytes). Generates XOR-based FEC (Forward Error Correction) packets (1 parity packet per 5 fragments).
4.  **Sender Thread**: Manages the UDP socket and broadcasts packets to all connected clients using a lock-free SPSC queue.

### Client Pipeline
1.  **Receiver Thread**: Listens for incoming UDP packets.
2.  **Reassembly & FEC**: Uses an O(1) `FixedRingBuffer` to track fragments. If a packet is lost, it attempts recovery using FEC parity.
3.  **Jitter Buffer**: An adaptive buffer that handles network variance. It calculates moving average jitter (EWMA) and maintains a minimal residence time to ensure smooth playback with minimal delay.
4.  **Decoder Thread**: Uses D3D11VA/DXVA2 hardware decoding to process H.264 bitstream directly into textures.
5.  **Render Thread**: Presents the decoded textures to the screen using a DXGI Flip Model swap chain with tearing support for minimum latency.

## Networking Protocol (Hardened UDP)

### Video Packet (Type 0x01)
- **Header (40 bytes)**: Includes `frameId`, `fragmentIndex`, `totalFragments`, `packetSequence`, `timestamp`, and `flags` (e.g., Keyframe).
- **Payload**: Raw H.264 fragment.

### FEC Packet (Type 0x05)
- **Header**: Includes `frameId`, `fragmentStart`, `fragmentCount`, and `dataSize`.
- **Payload**: XOR parity of the fragment group.

### Input Packet (Type 0x02)
- Supports Keyboard (RawInput), Mouse (Absolute/Relative), and Gamepad (XInput) data with delta-compression.

## Synchronization & Memory
- **Lock-Free Queues**: Hot paths use Single-Producer Single-Consumer (SPSC) queues to eliminate mutex contention.
- **Memory Pooling**: Pre-allocated pools for `Packet` and `FrameData` objects prevent runtime heap allocations during streaming.
