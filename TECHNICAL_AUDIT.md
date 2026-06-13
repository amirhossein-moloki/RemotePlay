# Parsec-lite Technical Audit (Production Grade)

## 1. Latency Analysis (End-to-End)

### Hot Path Flow:
1. **Capture (DXGI)**: ~1-2ms. Thread is semi-blocking on `AcquireNextFrame`.
2. **Queue (Capture -> Encoder)**: Sub-ms. `SafeQueue` uses `std::mutex` which is fine for 60-120fps, but could be a bottleneck at higher rates.
3. **Encode (NVENC/AMF)**: ~2-5ms. Zero-copy used (texture pointer passed).
4. **Queue (Encoder -> Network)**: Sub-ms.
5. **Packetization**: **Bottleneck Found.** Current implementation allocates a `std::vector<uint8_t>` for *every single UDP fragment*. At 5Mbps, this is ~500 allocations/sec.
6. **Network (LAN)**: ~1-2ms.
7. **Reassembly (Receiver)**: ~1ms. Uses `std::map` for sequence tracking (Log N).
8. **Decode (D3D11VA)**: ~2-5ms.
9. **Render (Present)**: ~1ms.

**Estimated Total**: 10-20ms (Target < 50ms met).

## 2. Memory Management & Copies

### Current Copies:
1. **Host**: Encoded bitstream -> `udpBuf` (Copy 1).
2. **Client**: Socket -> `buf` (Kernel Copy). `buf` -> `frame->buffer` (Copy 2).
3. **GPU**: Decoder Texture -> Backbuffer (Copy 3 via `CopyResource`).

### Issues:
- **Heap Allocations**: The Host network sender allocates memory in the hot path.
- **Buffer Recycling**: Client uses a `FrameData` pool, which is good. Host does not recycle fragment buffers.

## 3. Thread Safety & Contention

- `SafeQueue` is robust but uses `std::condition_variable`.
- `clientsMutex` in Host is held during UDP transmission. If one `SendTo` blocks (unlikely for UDP but possible in some OS stacks), it stalls the entire broadcast.

## 4. Packet Loss & Stress Resilience

### Strengths:
- Fragment-based reassembly allows partial frame reception (though H.264 usually needs the whole NAL unit).
- Stale frames are dropped by the `Receiver`.

### Weaknesses:
- **No FEC**: Loss of a single packet kills the entire frame.
- **No Jitter Buffer**: The system tries to render the latest completed frame immediately. Jitter in LAN will cause micro-stutter.
- **Congestion Control**: None. The system pumps 5000kbps regardless of packet loss.

## 5. Recommended Fixes for MVP+
1. **Pre-allocate Fragment Buffers**: Use a single large buffer or a pool for UDP packets.
2. **Replace std::map**: Use a fixed-size ring buffer for `m_pendingFrames`.
3. **Asynchronous Sending**: Move `SendTo` to a pool if targeting multiple clients.
