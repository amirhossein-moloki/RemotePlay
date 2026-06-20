# NexusDash: Encoder & Decoder Policy Specification

## 1. Encoder Manager Architecture

The Encoder Manager is the central orchestration layer for video compression in the NexusDash ecosystem. It manages the lifecycle of hardware and software encoders, ensuring optimal performance and absolute session stability.

### 1.1 Preflight Encoder Validation (PEV)
Before any streaming session commences, the system MUST execute a mandatory **Preflight Encoder Validation** phase. This phase ensures that the selected hardware is capable of sustaining the required workload before the session is established.

**Validation Order:**
1.  **NVENC (NVIDIA)**
2.  **QSV (Intel QuickSync)**
3.  **AMF (AMD)**
4.  **Software (libx264 fallback)**

**Validation Requirements:**
*   **Initialization Test:** The encoder must successfully open the hardware context and allocate necessary resources.
*   **Encode Test Frame:** A short sequence of test frames must be encoded to verify driver stability and throughput.
*   **Stability Check:** Verify that the encoder can maintain the target FPS without fatal errors.
*   **Resource Constraints:** The system checks GPU utilization and available VRAM to ensure operation within target parameters.

**Failure Rule:**
If an encoder fails any stage of the PEV, it is marked as unavailable. The system automatically attempts the next encoder in the priority list. If no encoders succeed, the stream MUST NOT start.

### 1.2 Encoder Session Lock
To ensure maximum stability, NexusDash enforces a strict **Encoder Session Lock** once streaming starts.

*   **Session-Persistent Backend:** The encoder backend (e.g., NVENC) selected during Preflight is LOCKED for the duration of the session.
*   **No Runtime Switching:** Switching between different hardware backends or between hardware and software is strictly prohibited during normal operation.
*   **Only Exception:** Emergency Encoder Fallback (see Section 3.2).

---

## 2. Quality Tier System & Runtime Adaptation

NexusDash utilizes a highly granular, per-client adaptation model to balance visual fidelity with network performance.

### 2.1 Per-Client Streaming Profile
Adaptation is performed strictly at the client level. The **Runtime ABR Controller** maintains an independent **Per-Client Streaming Profile** for every connected client.

**Per-Client Metrics:**
*   Bandwidth Estimation
*   Packet Loss Rate
*   Decode Performance
*   Frame Drop Rate
*   Device Capability (Hardware/Software Decoder support)

**Independent Adaptation:**
Each client receives an independent bitrate, resolution tier, and performance profile. **Global tiering is strictly forbidden.**

### 2.2 Quality Tiers (A-D)
| Tier | Resolution | Target Bitrate | Preset | Usage |
| :--- | :--- | :--- | :--- | :--- |
| **A** | Base (up to 4K) | 15.0 - 50.0 Mbps | Quality / P7 | High-performance LAN |
| **B** | Base (up to 1440p) | 8.0 - 15.0 Mbps | Balanced / P4 | Standard LAN / High-end WiFi |
| **C** | 720p / 1080p | 4.0 - 8.0 Mbps | Performance / P1 | Low-end WiFi / Congested LAN |
| **D** | 480p / 540p | 1.5 - 4.0 Mbps | Ultrafast | Recovery Mode / Critical Network |

### 2.3 Runtime ABR Controller
During an active session, the **Runtime ABR Controller** is the ONLY allowed mechanism for adaptation. It may dynamically adjust:
*   **Bitrate:** Adjusted based on bandwidth and packet loss.
*   **Resolution Scaling:** Transitioning between Quality Tiers (A through D).
*   **Encoding Preset Tuning:** Adjusting quality vs. performance tradeoffs.

**Forbidden:** Switching NVENC ↔ QSV ↔ AMF ↔ Software at runtime is forbidden.

---

## 3. Fallback & Switching Logic

### 3.1 Preflight Failure Handling
Failures during the **Preflight Encoder Validation** phase lead to an immediate promotion of the next available backend. If all backends fail, the session fails to start.

### 3.2 Emergency Encoder Fallback
The **Emergency Encoder Fallback** is a recovery mechanism triggered exclusively by fatal runtime crashes.

**Triggers:**
*   Encoder API Crash (e.g., GPU driver failure).
*   Hardware encoder becomes unusable/unresponsive.

**Procedure:**
1.  **Immediate Switch:** Move the session immediately to the **Software Encoder (libx264)**.
2.  **Preserve Session:** Maintain the network connection and client state.
3.  **Synchronization:**
    *   Trigger an immediate **Force Keyframe**.
    *   Send a **Stream Resync Signal** to the client.
    *   Align decoder reset to the new software-encoded IDR frame.

---

## 4. Decoder Architecture

The decoder is fully decoupled from the host's encoder selection, ensuring resilience and low-latency presentation.

### 4.1 Zero-Copy GPU Path
Where possible, the decoder utilizes a zero-copy path (e.g., D3D11VA) to deliver decoded frames directly to the GPU renderer.

### 4.2 Software Fallback
If hardware decoding is unavailable or fails, the system falls back to:
*   **libavcodec (Software)** → `sws_scale` → RGBA.
*   **D3D11 Upload:** Uploading the software buffer to the GPU for presentation.

---

## 5. Synchronization System

### 5.1 Recovery Synchronization
Following an **Emergency Encoder Fallback**, the system must:
*   Request an immediate Keyframe.
*   Ensure **SPS/PPS Repetition** is active on the new encoder.
*   Align the client-side decoder to the new stream parameters.

### 5.2 SPS/PPS Repetition
To ensure rapid client join and recovery after packet loss, the system enforces in-band repetition of Sequence and Picture Parameter Sets (SPS/PPS).

---
*Status: Approved for NexusDash Production Implementation*
*Architect: Senior Systems Architect AI*
