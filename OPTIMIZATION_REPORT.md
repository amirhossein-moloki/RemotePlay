# Parsec-lite Optimization Report: Summary of Gains

This report summarizes the optimization journey from the initial prototype to the production-grade C++ implementation.

## 📈 Latency Reduction

| Version | E2E Latency | Key Optimization |
| :--- | :--- | :--- |
| **Python Prototype** | ~150-200ms | N/A (Baseline) |
| **Early C++ Port** | ~60-80ms | Transition to native code, basic hardware encoding. |
| **Memory Optimized** | ~45-55ms | Pre-allocated packet pools, zero heap allocations. |
| **Shared Texture Path** | ~25-35ms | GPU-direct capture and encoding (shared D3D11 device). |
| **Networking Hardened**| **~15-20ms** | Lock-free SPSC queues, adaptive jitter buffer, XOR FEC. |

## ⚙️ Key Technical Breakthroughs

### 1. GPU Zero-Copy
Eliminating the CPU-GPU memory bottleneck was the single largest gain. By sharing a D3D11 device between DXGI and FFmpeg, we reduced "Encode Start" latency from ~12ms to ~1ms.

### 2. Lock-Free SPSC Queues
Standard `std::mutex` and `std::condition_variable` primitives caused unpredictable micro-stuttering (1-10ms spikes). Moving to lock-free ring buffers smoothed out frame delivery and significantly improved P99 metrics.

### 3. Adaptive Jitter Buffer (EWMA)
The transition from a fixed 33ms buffer to an adaptive 1-10ms buffer (based on real-time network variance) allowed us to reach "instant" feeling gameplay while remaining resilient to minor LAN fluctuations.

### 4. DXGI Flip Model & Tearing
By bypassing the DWM compositor on the client side, we eliminated the final "hidden" frame of latency (16.7ms @ 60Hz).
