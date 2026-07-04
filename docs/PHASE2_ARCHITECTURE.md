# Phase 2: Real-Time Media Streaming Architecture

## 1. High-Level Architecture Overview
The Parsec-Lite Phase 2 architecture focuses on integrating a low-latency audio pipeline, enhancing the existing video and input streams, and introducing a global synchronization engine. The system follows a distributed producer-consumer model where the Host captures, encodes, and transmits media in real-time, and the Client receives, decodes, and renders it with minimal jitter.

### Components
- **Host Engine:** Handles DXGI capture, WASAPI audio loopback, HW video encoding (NVENC/AMF), and Opus audio encoding.
- **Client Engine:** Handles HW video decoding, Opus audio decoding, D3D11 rendering, and WASAPI audio playback.
- **Secure Transport:** UDP-based layer with XChaCha20-Poly1305 encryption and sliding-window replay protection.
- **Sync Engine:** RTT-aware clock synchronization and timestamp-based A/V alignment.

---

## 2. Media Pipeline Design
The pipeline is designed for <30ms end-to-end latency using zero-copy paths and lock-free primitives.

**Capture → Encode → Packetize → Transport → Jitter Buffer → Decode → Render**

- **Video:** DXGI Desktop Duplication → D3D11 Texture → NVENC/AMF (Zero-copy) → Fragmented UDP packets.
- **Audio:** WASAPI Loopback → PCM Float32 → Opus (Low Latency Mode) → Encrypted UDP packets.
- **Transport:** Multiplexed secure UDP stream with FEC (Forward Error Correction) for video.

---

## 3. Audio Streaming Architecture
A dedicated low-latency audio pipeline running independently of the video thread.

- **Codec:** Opus (Complexity: 5-10, Bitrate: 64-128kbps, Frame Size: 10ms-20ms).
- **Sampling:** 48kHz Stereo.
- **Pipeline:**
  - **Host:** WASAPI Event-driven capture -> Ring Buffer -> Opus Encoder -> Secure Packetizer.
  - **Client:** Jitter Buffer -> Opus Decoder -> WASAPI Shared Mode (Event-driven).
- **Synchronization:** Audio packets carry the same global clock timestamps as video frames for drift correction.

---

## 4. Input System Design
Enhanced input handling to minimize "input-to-display" lag.

- **Events:** Keyboard, Mouse (Relative/Absolute), Gamepad (XInput).
- **Optimization:**
  - **Batching:** High-frequency mouse movements are batched into a single UDP packet per 4ms-8ms to avoid network overhead.
  - **Timestamps:** Every input event carries a microsecond-precision client timestamp to allow the host to compensate for network jitter during injection.
  - **Injection:** Host uses `SendInput` for mouse/KB and ViGEmBus for gamepad emulation.

---

## 5. Synchronization System
Ensures lip-sync and smooth playback through clock alignment.

- **Clock Sync:** Periodical RTT probes calculate the offset between Client and Host monotonic clocks.
- **A/V Alignment:** The `JitterBuffer` uses the synchronized timestamps to schedule frame presentation and audio playback.
- **Drift Correction:** Dynamic adjustment of audio playback speed (resampling) or minor frame pacing tweaks to stay within the target sync window (+/- 5ms).

---

## 6. Threading Model
Multi-threaded architecture to prevent blocking on the hot path.

### Host Threads:
- **Main/Session Thread:** Lifecycle management.
- **Capture & Video Encode:** DXGI affinity thread (highest priority).
- **Audio Capture & Encode:** High-priority real-time thread.
- **Network Sender:** Batch-send UDP packets from a lock-free queue.
- **Network Receiver:** Processes feedback, input, and clock sync probes.

### Client Threads:
- **Network Receiver:** Reassembles fragments and pushes to Jitter Buffer.
- **Main/Render Thread:** D3D11 Present and Video Decoding loop.
- **Audio Playback:** Independent high-priority thread for decoding and WASAPI output.
- **Input Capture:** Low-latency hook thread.

---

## 7. Latency Optimization Strategy
- **Zero-Copy:** GPU textures remain in VRAM from capture to encoder and from decoder to renderer.
- **Lock-Free Queues:** Use Single-Producer Single-Consumer (SPSC) queues for inter-thread communication on the media path.
- **ABR (Adaptive Bitrate):** Real-time bitrate adjustment based on RTT and packet loss feedback.
- **Frame Drop Strategy:** If the Jitter Buffer exceeds 2 frames, the client drops intermediate P-frames and requests an IDR (Keyframe) to reset latency.

---

## 8. Performance Bottlenecks & Mitigations
- **Bottleneck:** Network Jitter. **Mitigation:** Dynamic Jitter Buffer depth (0-15ms) based on variance.
- **Bottleneck:** HW Encoder saturation. **Mitigation:** ABR and Resolution Scaling.
- **Bottleneck:** Context Switching. **Mitigation:** Thread affinity and avoiding system calls in the hot loop.
- **Bottleneck:** OS Scheduling. **Mitigation:** Setting thread priorities to `THREAD_PRIORITY_TIME_CRITICAL`.
