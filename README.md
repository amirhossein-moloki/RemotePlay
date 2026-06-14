# Parsec-lite: High-Performance LAN Game Streaming

Parsec-lite is a low-latency, high-performance LAN game streaming system. It is a production-grade C++ implementation capable of sub-20ms end-to-end latency.

## ⚡ Quick Start

### Build & Run
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
# Host
./parsec-lite.exe --host
# Client
./parsec-lite.exe --client <HOST_IP>
```

## 🚀 Key Features
- **Ultra-Low Latency**: Target < 20ms E2E latency on standard Gigabit LAN.
- **High-Performance Capture**: GPU-direct frame access via Windows DXGI Desktop Duplication API.
- **Hardware Acceleration**: Full support for NVENC (NVIDIA), AMF (AMD), and QuickSync (Intel) encoding, with D3D11VA/DXVA2 hardware decoding.
- **Hardened Networking**: Custom UDP protocol with:
  - **FEC (Forward Error Correction)**: XOR-based parity for packet loss recovery.
  - **Adaptive Jitter Buffer**: Dynamically adjusts to network conditions to eliminate stutter.
  - **Lock-Free Queues**: SPSC (Single-Producer Single-Consumer) queues for zero-contention data flow.
- **Input Forwarding**:
  - **Keyboard/Mouse**: Low-latency Windows Raw Input capture and injection.
  - **Controllers**: Multi-client XInput support with ViGEmBus virtual controller injection.

## 🛠️ Requirements
- **OS**: Windows 10/11 (for DXGI and ViGEm)
- **Hardware**: NVIDIA/AMD/Intel GPU with hardware encoding support.
- **Dependencies**: FFmpeg (libavcodec), ViGEmBus driver.

## ⚖️ Performance Targets
| Component | Target Latency |
| :--- | :--- |
| Capture (DXGI) | 1-2ms |
| Encode (NVENC) | 3-5ms |
| Network (LAN) | 1-2ms |
| Decode & Render | 4-7ms |
| **Total E2E** | **~15-20ms** |

## 🛡️ License
MIT
