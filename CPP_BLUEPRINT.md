# Parsec-lite: Core Technical Pillars (C++ Reference)

This document serves as the technical reference for the core components of the Parsec-lite C++ implementation. These pillars are the foundation of our low-latency performance.

## 1. High-Performance Screen Capture
**Implementation**: `cpp/host/capture_dxgi.cpp`
- Uses **Windows Desktop Duplication API (DDA)**.
- Frame acquisition is synchronized with the GPU, providing direct access to the desktop's `ID3D11Texture2D`.
- Avoids expensive CPU-side `BitBlt` operations.

## 2. Hardware Video Encoding (Zero-Copy)
**Implementation**: `cpp/host/encoder_hw.cpp`
- Leverages **FFmpeg libavcodec** with `D3D11VA` hardware context.
- **The Secret Sauce**: The `ID3D11Texture2D` from the Capture module is shared directly with the encoder. No data is copied to the CPU or re-uploaded to the GPU.
- **Settings**:
    - Mode: CBR (Constant Bitrate)
    - Profile: High / Main
    - Tune: `zerolatency`
    - B-Frames: 0 (Disabled to eliminate GOP delay)

## 3. Custom UDP Protocol & FEC
**Implementation**: `cpp/common/protocol.hpp`, `cpp/client/receiver.cpp`
- Standard UDP is used to avoid TCP's head-of-line blocking.
- **Packetization**: Frames are split into ~1300-byte fragments.
- **XOR FEC**: For every 5 video packets, we generate 1 parity packet. This allows recovery of any single lost packet per group with zero additional latency (unlike retransmission).

## 4. Virtual Controller Injection
**Implementation**: `cpp/host/input_injector.cpp`
- Uses the **ViGEmClient SDK** to talk to the **ViGEmBus** kernel driver.
- Supports emulating Xbox 360 and DualShock 4 controllers.
- Multi-client mapping: Each unique client IP + ID is mapped to a distinct virtual controller slot on the host.

## 5. Client Rendering & Flip Model
**Implementation**: `cpp/client/renderer_d3d11.cpp`
- Uses **Direct3D 11** for presentation.
- **DXGI Flip Model**: Employs `DXGI_SWAP_EFFECT_FLIP_DISCARD`.
- **Tearing Support**: When VSync is off, we use `DXGI_PRESENT_ALLOW_TEARING` to deliver frames to the monitor as fast as they are decoded, bypassing the Windows DWM compositor's extra frame of lag.

## 6. Jitter Buffer & Frame Pacing
**Implementation**: `cpp/client/jitter_buffer.cpp`
- Reorders out-of-order UDP packets.
- Implements an **Adaptive Depth** algorithm.
- Uses EWMA (Exponentially Weighted Moving Average) to track network variance and adjust the target residence time (1ms to 10ms) dynamically.
