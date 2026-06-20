# NexusDash: Encoder & Decoder Policy Specification

## 1. Encoder Manager Architecture

The Encoder Manager is the central orchestration layer for video compression in the NexusDash ecosystem. Its primary responsibility is to manage the lifecycle of hardware and software encoders, ensuring optimal performance and stability.

### 1.1 Preflight Encoder Validation (PEV)
Before any streaming session commences, the system MUST execute a mandatory **Preflight Encoder Validation** phase. This phase ensures that the selected hardware is capable of sustaining the required workload before the user begins their session.

**Validation Order:**
1.  **NVENC (NVIDIA)**
2.  **QSV (Intel QuickSync)**
3.  **AMF (AMD)**
4.  **Software (libx264 fallback)**

**Preflight Requirements:**
*   **Initialization Test:** The encoder must successfully open the hardware context and allocate necessary resources.
*   **Encode Stability Cycle:** A short sequence of test frames must be encoded to verify driver stability and throughput.
*   **Resource Constraints:** The system checks for GPU utilization and available VRAM to ensure the encoder can operate within the target parameters.

**Failure Handling:**
If an encoder fails any stage of the PEV, it is marked as `Unavailable for Session`. The system automatically moves to the next encoder in the priority list. If no encoders pass validation, the session startup is aborted with a critical hardware error.

### 1.2 Encoder Session Lock
To ensure session stability and prevent glitches associated with runtime hardware context switching, NexusDash enforces a strict **Encoder Session Lock**.

*   **Lock-in:** Once the session begins with a validated encoder, that encoder is locked for the duration of the stream.
*   **Transition Restriction:** Switching between different hardware backends (e.g., NVENC to QSV) or between hardware and software is strictly prohibited during normal operation.

---

## 2. Quality Tiers & Runtime Adaptation

NexusDash utilizes a multi-tiered quality system to balance visual fidelity with network performance.

### 2.1 Per-Client Streaming Profile
A core principle of NexusDash is that adaptation is performed at the client level. The **Runtime ABR Controller** maintains an independent **Per-Client Streaming Profile** for every connected client.

**Per-Client Metrics:**
*   Estimated Network Bandwidth
*   Real-time Packet Loss Rate
*   Average Round Trip Time (RTT)
*   Client Decode Latency & Frame Drop Rate
*   Device Hardware Capabilities (Decoder type, Resolution support)

### 2.2 Quality Tiers (A-D)
| Tier | Resolution | Target Bitrate | Preset | Usage |
| :--- | :--- | :--- | :--- | :--- |
| **A** | Base (up to 4K) | 15.0 - 50.0 Mbps | Quality / P7 | High-performance LAN |
| **B** | Base (up to 1440p) | 8.0 - 15.0 Mbps | Balanced / P4 | Standard LAN / High-end WiFi |
| **C** | 720p / 1080p | 4.0 - 8.0 Mbps | Performance / P1 | Low-end WiFi / Congested LAN |
| **D** | 480p / 540p | 1.5 - 4.0 Mbps | Ultrafast | Recovery Mode / Critical Network |

### 2.3 Runtime ABR (Adaptive Bitrate)
During an active session, the **Runtime ABR Controller** may dynamically adjust only the following parameters:
*   **Bitrate:** Seamlessly adjusted based on network congestion.
*   **Resolution Scaling:** Switching between defined Quality Tiers (A through D).
*   **Encoding Preset:** Adjusting the performance/quality tradeoff (e.g., P7 to P4).

**Forbidden Changes:** Runtime switching of the encoder backend (NVENC ↔ QSV ↔ AMF) is explicitly forbidden outside of emergency recovery.

---

## 3. Fallback & Switching Logic

### 3.1 Preflight Failure Handling
Failures during the **Preflight Encoder Validation** phase lead to an immediate promotion of the next available backend. The user is notified only if a significant downgrade occurs (e.g., falling back to Software before the session starts).

### 3.2 Emergency Encoder Fallback
The **Emergency Encoder Fallback** is a recovery-only mechanism triggered exclusively by fatal runtime failures.

**Triggers:**
*   GPU Driver Crash / Device Removal.
*   Fatal Encoder API Error (e.g., `AVERROR_EXTERNAL`).
*   Persistent instability detected by the watchdog.

**Recovery Procedure:**
1.  Immediately terminate the failed hardware context.
2.  Switch to the **Software Encoder (libx264)**.
3.  Issue a **Force Keyframe** request to resynchronize all clients.
4.  Signal a client-side decoder reset if necessary to maintain session continuity.

---

## 4. Decoder Architecture

The decoder is decoupled from the host's encoder logic, designed for maximum resilience and low-latency presentation.

### 4.1 Zero-Copy Pipeline
Where supported, the decoder utilizes a zero-copy GPU path (D3D11VA / DXVA2) to deliver decoded frames directly to the renderer's swapchain, minimizing PCI-E bus transfers.

### 4.2 Software Decoder Fallback
If the hardware decoder fails or the stream switches to a format unsupported by the client's GPU, the system transparently falls back to a software path:
*   **libavcodec (Software)** → `sws_scale` → RGBA conversion.
*   **Texture Upload:** The resulting frame is uploaded to a D3D11 texture for final presentation.

### 4.3 Resilience Features
*   **IDR Alignment:** Decoder resynchronizes on the next IDR frame following any encoder switch or packet loss burst.
*   **Reference Frame Management:** Aggressive dropping of P-frames if the reference frame is missing, preventing "smearing" artifacts.

---

## 5. Synchronization & Latency Control

### 5.1 Keyframe Synchronization
Keyframes are requested dynamically by the **Session Manager** in response to:
*   New client connection.
*   Emergency Encoder Fallback.
*   High packet loss causing reference frame corruption.

### 5.2 Latency Management
*   **Jitter Buffer:** An adaptive exponentially weighted moving average (EWMA) buffer manages network variations.
*   **Frame Timestamps:** Strict capture-to-present timestamp tracking (Telemetry) ensures sub-20ms E2E targets are monitored and maintained.

---
*Status: Approved for NexusDash Production Implementation*
*Architect: Senior Systems Architect AI*
