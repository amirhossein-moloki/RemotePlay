# Latency Optimization Strategies: Implementation Details

Parsec-lite achieves sub-20ms latency through a combination of hardware-level integration and software-level micro-optimizations.

## 1. Video Pipeline (The "Zero-Copy" Path)
- **Capture (DXGI DDA)**: We use the `AcquireNextFrame` method of `IDXGIOutputDuplication`. This provides a pointer to the frame in GPU memory.
- **Unified D3D11 Device**: The Capture, Encoder, and Renderer modules all share the same `ID3D11Device`. This allows sharing textures between components via `ID3D11Texture2D` pointers without costly `Staging` copies or GPU-to-CPU transfers.
- **FFmpeg HW Acceleration**: We use `av_hwdevice_ctx_create` with `AV_HWDEVICE_TYPE_D3D11VA`. This ensures the encoder (NVENC/AMF) reads directly from the captured texture.

## 2. Encoder Tuning (The "Zerolatency" Preset)
The encoder is configured with specific flags to minimize lookahead and buffering:
- `tune=zerolatency`: Disables B-frames and reduces internal frame buffering.
- `preset=ultrafast`: Prioritizes encoding speed over compression efficiency.
- `gop_size=infinite`: Prevents periodic I-frame spikes, using a single initial I-frame and subsequent P-frames. Intra-refresh is used for error recovery.
- `rc=cbr`: Constant Bitrate ensures predictable network traffic and avoids bufferbloat.

## 3. Networking (Custom UDP Protocol)
- **MTU Alignment**: Packets are capped at 1300 bytes (below the 1500-byte MTU) to prevent IP fragmentation, which can cause significant latency in some network stacks.
- **Lock-Free SPSC Queues**: Thread-to-thread communication (e.g., Encoder -> Sender) uses Single-Producer Single-Consumer queues, avoiding mutex overhead and kernel-level waits.
- **XOR FEC**: Forward Error Correction recovers lost packets instantly without waiting for RTT-sensitive retransmissions (NACK).

## 4. Client Presentation (The "Flip Model")
- **DXGI Flip Discard**: Uses `DXGI_SWAP_EFFECT_FLIP_DISCARD` to bypass the legacy BitBlt copy in the Windows compositor.
- **Allow Tearing**: When VSync is disabled, we use `DXGI_PRESENT_ALLOW_TEARING`. This allows the GPU to present the latest decoded frame immediately to the monitor, even mid-refresh, for the absolute lowest input-to-display latency.

## 5. Precise Timing & Telemetry
Every frame is tagged with a `high_resolution_clock` timestamp at the following stages:
1.  `CaptureTimestamp`
2.  `EncodeStartTimestamp`
3.  `EncodeEndTimestamp`
4.  `ReceiveTimestamp`
5.  `DecodeTimestamp`
6.  `PresentTimestamp`
This allows real-time monitoring of every millisecond in the pipeline.
