# Parsec-lite C++ Implementation

This directory contains the core high-performance C++ source code for the Parsec-lite LAN Game Streaming system.

## 🚀 Key Features
- **High-Performance Capture**: DXGI Desktop Duplication API for GPU-direct frame access.
- **Hardware Acceleration**: Out-of-the-box support for NVENC, AMF, and QuickSync via FFmpeg.
- **Low-Latency Input**: High-precision Windows Raw Input capture and kernel-level ViGEmBus injection.
- **Unified Executable**: A single binary that handles both Host and Client roles.
- **Adaptive Networking**: XOR-based FEC and an adaptive jitter buffer to handle LAN fluctuations.

## 🛠️ Build Requirements
- **OS**: Windows 10/11 (Required for DXGI, SendInput, and ViGEmBus).
- **Compiler**: Visual Studio 2019+ or MinGW-w64 with C++17 support.
- **Build System**: CMake 3.10+.
- **Dependencies**:
  - **FFmpeg**: Required for `libavcodec`, `libavutil`. Ensure it's compiled with hardware acceleration support.
  - **ViGEmClient**: Required for virtual controller support.
  - **DirectX SDK**: Included with modern Windows SDK.

## 🏗️ Building
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## 📖 Usage

### 1. List Network Interfaces
Before starting, identify which network interface to use:
```bash
./parsec-lite.exe --list
```

### 2. Start Host
Start the host on the machine you want to stream from:
```bash
# Default active interface
./parsec-lite.exe --host

# Specific interface (e.g., index 1)
./parsec-lite.exe --host --iface 1
```

### 3. Start Client
Start the client on the machine you want to play on:
```bash
# Connect to host IP
./parsec-lite.exe --client 192.168.1.50
```

## 🏗️ Architecture Notes
- **Zero-Copy**: Passes `ID3D11Texture2D` pointers directly from capture to encoder, avoiding the CPU entirely.
- **UDP Fragmenting**: Optimizes MTU usage by fragmenting large video frames into ~1300-byte packets.
- **SPSC Queues**: Threading is managed via lock-free Single-Producer Single-Consumer queues to eliminate mutex contention lag.
