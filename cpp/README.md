# Parsec-lite C++ Implementation

This directory contains the production-grade C++ source code for the LAN Game Streaming System.

## Features
- **High-Performance Capture**: DXGI Desktop Duplication API for GPU-direct frame access.
- **Hardware Acceleration**: Architectural support for NVENC, AMF, and QuickSync via FFmpeg.
- **Low-Latency Input**: Windows Raw Input capture and `SendInput`/`ViGEmBus` injection.
- **Unified Executable**: Single binary for both Host and Client modes.
- **Interface Selection**: Explicit control over which network adapter to use.

## Project Structure
- `cpp/common/`: Shared networking and protocol definitions.
- `cpp/host/`: Screen capture, hardware encoding, and input injection logic.
- `cpp/client/`: Video receiving, reassembly, hardware decoding, and input capture.
- `cpp/main.cpp`: Entry point with mode selection and initialization logic.

## Build Requirements
- **OS**: Windows 10/11 (Required for DXGI, SendInput, ViGEmBus).
- **Compiler**: Visual Studio 2019+ or MinGW-w64.
- **Build System**: CMake 3.10+.
- **Dependencies**:
  - **FFmpeg**: Required for `libavcodec`, `libavutil`.
  - **ViGEmClient**: Required for virtual controller support.
  - **DirectX SDK**: Included with modern Windows SDK.

## Building
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Usage

### 1. List Interfaces
```bash
./parsec-lite.exe --list
```

### 2. Start Host
```bash
# Starts host on the default active interface
./parsec-lite.exe --host

# Starts host on a specific interface (e.g., interface index 1)
./parsec-lite.exe --host --iface 1
```

### 3. Start Client
```bash
# Connects to the host at 192.168.1.50
./parsec-lite.exe --client 192.168.1.50
```

## Architecture Notes
- **Zero-Copy**: The system is designed to pass `ID3D11Texture2D` pointers directly from DXGI to the hardware encoder, avoiding CPU-GPU memory copies.
- **UDP Protocol**: Uses a custom sequence-aware protocol with packet fragmentation to stay within LAN MTU (1500 bytes).
- **Jitter Buffer**: The client reassembles frames and handles out-of-order packets using a minimal buffer to prioritize latency over perfect smoothness.
