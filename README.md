# Parsec-lite: High-Performance LAN Game Streaming

Parsec-lite is a low-latency, high-performance LAN game streaming system. Originally started as a Python prototype, it has evolved into a production-grade C++ implementation capable of sub-20ms end-to-end latency.

## 🚀 Key Features (C++ Implementation)
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

## 🗺️ Documentation Map
- **[Architecture](ARCHITECTURE.md)**: High-level system design and multi-threaded pipeline.
- **[Design Deep-Dive](DESIGN.md)**: Technical details on networking, memory pooling, and threading.
- **[Optimization Strategies](OPTIMIZATION.md)**: How we achieved sub-20ms latency.
- **[Audit & Reports]**:
  - [Technical Audit](TECHNICAL_AUDIT.md)
  - [Performance Audit](PERFORMANCE_AUDIT.md)
  - [Optimization Report](OPTIMIZATION_REPORT.md)
  - [Final Project Status](AUDIT_REPORT.md)

## 🛠️ Getting Started (C++)
The C++ implementation is the primary version of Parsec-lite.

### Requirements
- **OS**: Windows 10/11 (for DXGI and ViGEm)
- **Hardware**: NVIDIA/AMD/Intel GPU with hardware encoding support.
- **Dependencies**: FFmpeg (libavcodec), ViGEmBus driver.

### Build & Usage
See **[cpp/README.md](cpp/README.md)** for detailed build and execution instructions.

## 🐍 Legacy Python Prototype
The original Python implementation is still available in the `host/` and `client/` directories for educational purposes or quick testing on non-Windows platforms (limited features).

### Quick Start (Python)
```bash
pip install -r requirements.txt
# Host
python3 host/streamer.py
# Client
python3 client/client.py <HOST_IP>
```

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
