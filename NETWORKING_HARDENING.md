# Parsec-lite Networking Layer Hardening

The networking layer is designed to handle the high throughput and strict latency requirements of 60+ FPS game streaming.

## 1. Architecture Overview

### Host: Multi-Threaded Broadcast
The host employs a dedicated **Sender Thread** that pops packets from a lock-free queue and broadcasts them to all active clients. This ensures that a slow client or network interface doesn't stall the encoding pipeline.

### Client: Adaptive Reassembly
The client's **Receiver Thread** reassembles fragmented frames into a `FixedRingBuffer`. This allows O(1) random access to frame fragments, which is essential for out-of-order UDP delivery.

## 2. Key Hardening Features

### 2.1. Forward Error Correction (FEC)
We use XOR-based FEC to combat packet loss without retransmissions:
- **Encoding**: For every group of fragments, we generate a parity packet: `P = F1 ^ F2 ^ ... ^ Fn`.
- **Recovery**: If any single fragment $Fi$ is missing, it is reconstructed: `Fi = P ^ F1 ^ ... ^ F(i-1) ^ F(i+1) ^ ... ^ Fn`.
- **Latency Impact**: Zero (no retransmission wait).

### 2.2. Adaptive Jitter Buffer
The `JitterBuffer` implementation in `cpp/client/jitter_buffer.cpp` manages the playback pace:
- **Jitter Tracking**: Measures the variance in inter-arrival times.
- **Dynamic Residence Time**: Automatically adjusts the buffer depth (e.g., between 1ms and 15ms) based on current network stability.
- **Fast Catch-up**: If the buffer exceeds a certain depth, it aggressively drops stale frames to prioritize real-time interaction over smoothness.

### 2.3. Zero-Allocation Memory Management
To prevent latency spikes caused by the OS memory allocator (Heap Fragmentation/GC), we use:
- **Packet Pools**: Pre-allocated pools for both the Host (UDP sender) and Client (UDP receiver).
- **Static Buffer Reuse**: `FrameData` objects are cycled through a pool rather than being created and destroyed for every frame.

### 2.4. Congestion Feedback
The client sends periodic `Feedback` packets (Type 0x06) to the host, containing:
- `lossRate`: Percentage of packets lost in the last interval.
- `rttMs`: Calculated Round-Trip Time.
- `lastReceivedFrameId`: Helps the host prune its own internal buffers.

## 3. Protocol Safety
- **Magic Bytes & Versioning**: All packets are validated for a specific protocol version to prevent cross-version crashes.
- **Sequence Verification**: Packets with `packetSequence` older than the current window are dropped immediately to prevent replay or stale data processing.
