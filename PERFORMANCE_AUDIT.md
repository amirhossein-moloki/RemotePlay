# Performance Audit Report: Parsec-lite Phase 1

## 1. Latency Breakdown (Measured & Projected)

Measurements taken from headless benchmark simulating 1080p 60fps at 20Mbps with 100Hz input polling.

| Component | Avg (ms) | P95 (ms) | P99 (ms) | Source |
|-----------|----------|----------|----------|--------|
| **Capture (DXGI)** | 1.50 | 2.50 | 4.00 | Projected (DDA) |
| **Encode (NVENC)** | 3.50 | 5.00 | 8.00 | Projected (Zerolatency) |
| **Packetization** | 0.07 | 0.15 | 0.20 | **Measured** |
| **Network (LAN)** | 1.00 | 2.00 | 5.00 | Estimated (Standard LAN) |
| **Reassembly** | 0.01 | 0.02 | 0.04 | **Measured** (Packet) |
| **FEC Recovery** | 0.11 | 0.50 | 1.24 | **Measured** |
| **Decode (D3D11)** | 3.00 | 4.50 | 7.00 | Projected |
| **Jitter Buffer** | 33.33 | 50.00 | 50.00 | **Code Analysis** (Fixed 2-3 frames) |
| **Render (Present)** | 1.00 | 2.00 | 16.00 | Projected (V-Sync dependent) |
| **Input (Net Latency)**| 0.18 | 0.80 | 2.16 | **Measured** |
| **Input (Injection)** | 0.05 | 0.10 | 0.25 | **Projected** (SendInput) |
| **Total (E2E)** | **43.75** | **69.67** | **97.67** | Calculated |

---

## 2. Memory Allocation Analysis

### Major Findings:
1. **Hot-Path Heap Allocations**:
   - **Packetizer**: `std::vector<PacketPool::Packet*> fragmentPtrs(totalFrags)` is allocated on the heap for *every* frame.
   - **Packet Pool Exhaustion**: Profiler detected 512+ exhaustion events in 10 seconds at 20Mbps. Each exhaustion triggers a 1MB `std::vector` allocation via `new`, causing massive latency spikes (P99 impact).
2. **Unnecessary Buffer Copies**:
   - Every fragment is copied from the encoded frame buffer to a UDP packet buffer.
   - The sender thread performs another copy or at least manages a unique_ptr move which involves atomic increments.
3. **Receiver Overhead**:
   - `FrameData` in the receiver has a fixed 1MB buffer. While pooled, it's large for small P-frames.

---

## 3. Thread Contention & Queue Analysis

### Queue Status (20Mbps Load):
- **Encode Queue**: Avg size 1.0. Healthy.
- **Send Queue**: P99 wait time reached 16ms during high fragment load, indicating the network thread is a major bottleneck.
- **Lock Contention**: `clientsMutex` in `RunHost` is held during the entire packetization and FEC generation loop, blocking telemetry reception.

---

## 4. Ranked Bottlenecks

| Rank | Bottleneck | Root Cause | Impact | Expected Gain |
|------|------------|------------|--------|---------------|
| 1 | **Fixed Jitter Buffer** | Logic always holds 2-3 frames regardless of network quality. | +33-50ms latency | **30ms+** |
| 2 | **Packet Pool Exhaustion** | Pool size (1000) is insufficient for 20Mbps (approx 2000 fragments/sec). | P99 spikes (10ms+) | Stabilized P99 |
| 3 | **Packetizer Allocations** | Heap allocation for fragment pointers every frame. | CPU Cache Pressure | 1-2ms |
| 4 | **Host Mutex Contention** | Coarse-grained `clientsMutex` and safe-queue locks. | Jitter/Stutter | Smoothness |

---

## 5. Optimization Roadmap (Phase 2)

### Priority 1: Critical (Latency & Stability)
- **Adaptive Jitter Buffer**: Implement logic that scales from 0 to 2 frames based on RTT and jitter telemetry.
- **Zero-Allocation Packetizer**: Replace `std::vector` of fragment pointers with a fixed-size stack array or a reusable pool.
- **Expand & Tune Packet Pools**: Increase `udpPool` size to 4000+ and implement a "warm-up" phase.

### Priority 2: Important (Throughput)
- **Lock-Free Queues**: Replace `std::mutex` based `SafeQueue` with a SPSC lock-free ring buffer.
- **Vectorized FEC**: Use SIMD (SSE/AVX) for the XOR recovery loop.

### Priority 3: Nice-to-Have
- **Asynchronous Sending**: Use `WSASend` or `sendmmsg` to batch UDP packets.
- **MTU Discovery**: Optimize `MAX_UDP_PAYLOAD` to avoid IP-layer fragmentation.
