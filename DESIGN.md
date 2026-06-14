# Parsec-lite Technical Design

## 1. High-Performance Streaming Design
The C++ implementation is designed for maximum throughput and minimum latency by focusing on three pillars: **Zero-Copy GPU Path**, **Lock-Free Concurrency**, and **Zero-Allocation Hot Paths**.

### 1.1. Zero-Copy GPU Path
- **Host**: Captured DXGI textures (`ID3D11Texture2D`) are passed directly to the hardware encoder (NVENC/AMF) via FFmpeg's `hwcontext_d3d11va`. No staging or CPU copies occur.
- **Client**: Decoded frames stay in GPU memory and are presented using a Shared Device context between the decoder and the D3D11 renderer.

### 1.2. Lock-Free Concurrency
To avoid OS-level thread contention and context switching:
- **SPSC Queues**: We use custom Single-Producer Single-Consumer ring buffers for cross-thread communication (e.g., Capture -> Encoder, Encoder -> Packetizer).
- **Atomic State**: Connection management and telemetry use atomic variables to avoid heavy mutex locks.

### 1.3. Zero-Allocation Hot Paths
- **Memory Pooling**: The `PacketPool` and `FrameDataPool` pre-allocate all necessary memory on startup.
- **Fragment Management**: No `std::vector` resizing or `new`/`delete` calls occur while streaming is active.

## 2. Hardened Networking Layer

### 2.1. UDP Protocol & Fragmentation
Frames are fragmented to 1300 bytes to stay safely within the standard LAN MTU (1500 bytes), preventing IP-layer fragmentation. Each packet includes a global sequence number for jitter calculation.

### 2.2. Forward Error Correction (FEC)
We implement XOR-based parity. For every $N$ fragments (default 5), an additional parity packet is sent.
- $P = F_1 \oplus F_2 \oplus F_3 \oplus F_4 \oplus F_5$
- If $F_3$ is lost, it is recovered as $F_3 = P \oplus F_1 \oplus F_2 \oplus F_4 \oplus F_5$.
- This eliminates the need for NACK/Retransmission cycles, which would add massive latency spikes.

### 2.3. Adaptive Jitter Buffer
The client tracks the inter-arrival time of packets. It uses an Exponentially Weighted Moving Average (EWMA) to estimate network jitter.
- **Residence Time**: Frames are held for a calculated minimal time to absorb jitter.
- **Catch-up**: If the buffer grows too deep (due to a network stall), the client drops older incomplete frames and skips to the latest I-frame to restore low latency.

## 3. Input System

### 3.1. Windows Raw Input
The client uses `RegisterRawInputDevices` to capture high-precision mouse and keyboard data. This avoids the lag and filtering inherent in the standard Windows message loop.

### 3.2. Virtual Controller Injection
The host uses the **ViGEmBus** driver. When a client connects, it is assigned a virtual Xbox 360 controller. XInput state is sent over UDP and injected into the target OS at the kernel level.

## 4. Performance Metrics (LAN Target)
- **Target FPS**: 60 / 120 / 144
- **Target Bitrate**: 20 - 50 Mbps
- **Target Latency**: 10-20ms
