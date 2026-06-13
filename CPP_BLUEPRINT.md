# Production C++ Design Blueprint for LAN Game Streaming

For a production-grade system like Parsec, C++ is the recommended language to achieve sub-16ms (one frame at 60fps) latency. Below is the architectural blueprint.

## 1. High-Performance Screen Capture (Windows)
Use the **Windows Desktop Duplication API (DDA)**. It provides direct access to the desktop frame in GPU memory (as a `ID3D11Texture2D`).

```cpp
// Pseudocode for DDA Capture
IDXGIOutputDuplication* desc;
DXGI_OUTDUPL_FRAME_INFO frameInfo;
IDXGIResource* desktopResource;
desc->AcquireNextFrame(0, &frameInfo, &desktopResource);
// Map desktopResource to a D3D11 Texture
```

## 2. Hardware Video Encoding
Use **NVIDIA NVENC SDK**, **AMD AMF**, or **Intel QuickSync**.
- **Crucial**: Perform "Zero-Copy" by passing the D3D11 texture pointer directly to the encoder. This avoids CPU-GPU memory transfers.
- **Settings**:
    - Rate Control: CBR (Constant Bitrate)
    - Preset: P1 (Lowest Latency)
    - Tuning: Ultra Low Latency
    - B-Frames: 0
    - GOP: Infinite (use periodic intra-refresh if needed)

## 3. Networking (UDP/ENet)
Standard sockets or a library like **ENet** or **GameNetworkingSockets (Valve)**.
- **Protocol**: Custom UDP protocol.
- **Forward Error Correction (FEC)**: Highly recommended for LAN to recover from occasional packet loss without retransmission.
- **MTU Management**: Ensure packets are < 1500 bytes to avoid fragmentation.

## 4. Input Injection (Host)
Use **ViGEmBus** for virtual gamepad support (Xbox 360 / DualShock 4).
For Keyboard/Mouse: Use `SendInput` API, or a kernel-level driver for bypass-resistant games.

```cpp
// ViGEm example
PVIGEM_CLIENT client = vigem_alloc();
PVIGEM_TARGET pad = vigem_target_x360_alloc();
vigem_connect(client);
vigem_target_add(client, pad);
// Send report
vigem_target_x360_update(client, pad, report);
```

## 5. Client Rendering
Use **SDL2** with **Direct3D 11** or **Vulkan** backend.
- Use **FFmpeg (libavcodec)** with hardware acceleration enabled (`dxva2` or `d3d11va`).
- Goal: Decode directly into a texture and render with the GPU.

## 6. Latency Pipeline
1. **Capture**: 1-2ms (DDA)
2. **Encode**: 2-5ms (NVENC)
3. **Network**: 1-2ms (LAN)
4. **Decode**: 2-5ms (D3D11VA)
5. **Render**: 1ms (Present)
**Total Target**: ~7ms to 15ms.
