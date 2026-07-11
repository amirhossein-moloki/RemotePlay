# Parsec-Lite: High-Performance Remote Play Engine

Parsec-Lite is a low-latency, production-grade remote play system built in C++20. It delivers a high-fidelity gaming experience over LAN and WAN by integrating hardware-accelerated video pipelines with an AI-driven optimization layer.

## Overview
Parsec-Lite is designed for ultra-low latency (sub-20ms E2E) desktop and game streaming. It utilizes the Windows DXGI Desktop Duplication API for high-frequency frame capture and leverages modern GPU encoders (NVENC, QSV, AMF) for sub-5ms encoding. The system is secured with modern cryptographic primitives and optimized by a Phase 5 AI layer that proactively adjusts stream parameters based on network conditions.

## Architecture Summary
The system follows a modular C++ architecture with clear separation between the core engine and UI layers:
*   **Core Engine (`cpp/`)**: Handles capture, encoding, networking, security, and AI optimization.
*   **NexusDash (`NexusDash/`)**: The modern production UI built with Qt 6 and QML, offering real-time telemetry and session management.
*   **ParsecLite.UI (`ParsecLite.UI/`)**: A legacy .NET-based prototype UI.
*   **Protocol Layer**: A custom UDP-based protocol with integrated FEC, clock synchronization, and multi-client support.

## System Components

### 1. Media Pipeline
*   **Capture**: DXGI Desktop Duplication with support for resolution scaling.
*   **Video Encoding**: Hardware-accelerated H.264/HEVC via FFmpeg (NVENC, QSV, AMF) and software fallback (libx264).
*   **Audio**: Loopback capture via WASAPI, Opus encoding/decoding, and shared-mode playback.
*   **Decoding**: Hardware-accelerated decoding via D3D11VA.

### 2. AI Optimization Layer (Phase 5)
*   **Latency Predictor**: EWMA-based forecasting of RTT and packet loss.
*   **Stream Engine**: Proactive Adaptive Bitrate (ABR) and resolution scaling.
*   **Intelligent Router**: Priority-based ranking of network path candidates.
*   **Input Predictor**: Client-side cursor forecasting (16ms ahead) to mask stream latency.

### 3. Networking & Reliability
*   **Custom UDP Protocol**: Optimized for SPSC (Single-Producer Single-Consumer) lock-free data flow.
*   **FEC (Forward Error Correction)**: XOR-based parity recovery for lost packets.
*   **Jitter Buffer**: Adaptive client-side buffer for smooth presentation.
*   **Clock Sync**: NTP-style 4-timestamp exchange for synchronized jitter buffer management.
*   **WAN Enablement**: Foundational STUN (RFC 5389) for reflexive candidate discovery.

### 4. Security Model
*   **Handshake**: Ephemeral X25519 key exchange.
*   **Encryption**: XChaCha20-Poly1305 AEAD for all stream data (Video, Audio, Input, Feedback).
*   **Identity**: Simple username-based identification with session ID isolation.
*   **Replay Protection**: Bitmask-based sliding window (size 1024) to prevent UDP replay attacks.

## Communication Flow
1.  **Handshake**: Client discovers local/reflexive candidates (STUN) and initiates a secure handshake with the Host.
2.  **Key Exchange**: Peers derive RX/TX session keys via X25519.
3.  **Capture & Encode**: Host captures desktop frames, which are encoded and fragmented.
4.  **Packetization**: Fragments are encrypted and transmitted via UDP with interleaved FEC packets.
5.  **Reassembly & Decode**: Client validates sequence, reassembles frames (with FEC recovery if needed), and decodes via GPU.
6.  **Feedback Loop**: Client sends real-time telemetry (RTT, loss, decode time) to the Host for AI-driven adaptation.

## Installation / Run Instructions

### Prerequisites
*   **OS**: Windows 10/11 (Host requires GPU with hardware encoding).
*   **Drivers**: [ViGEmBus](https://github.com/ViGEm/ViGEmBus/releases) (for controller support).
*   **Dependencies**: FFmpeg (Shared), libsodium.

### Automated Setup
```powershell
.\setup_deps.ps1
mkdir build && cd build
cmake .. -DBUILD_NEXUSDASH=ON
cmake --build . --config Release
```

### Manual Execution (CLI)
*   **Host**: `parsec-lite.exe --host`
*   **Client**: `parsec-lite.exe --client <HOST_IP>`
*   **Standalone (Testing)**: `parsec-lite.exe --standalone`

## Configuration
Configuration is managed via `config.ini` or the `ParsecConfig` API structure:
*   `bitrate`: Target bitrate in Kbps.
*   `fps`: Target capture/encode frame rate.
*   `useHardwareEncoding`: Boolean to toggle GPU acceleration.
*   `resolutionScale`: 0.1 to 1.0 multiplier for capture resolution.

## Limitations (Code Reality)
*   **Relay Service**: While the protocol supports `RelayData` headers, there is no built-in Relay server implementation in this repository.
*   **Linux Support**: The core engine contains Linux-compatible networking, but the capture (DXGI), injection (Win32/ViGEm), and rendering (D3D11) paths are Windows-only.
*   **Multi-Monitor**: Current implementation focuses on the primary display.
*   **Cloud Scaling**: Global infrastructure components (K8s, Orchestration) are documented in architectural phases but not present in the current codebase.

## Current Status
*   **Phase 1-2 (Security & Media)**: Fully Implemented.
*   **Phase 3 (WAN)**: Foundational (STUN/Candidate Discovery).
*   **Phase 5 (AI Optimization)**: Fully Implemented and integrated.

---

# Documentation Update Report

## 1. What changed from old README
*   **Updated Scope**: Reflected that the project is now a multi-component system (NexusDash + Core) rather than just a CLI tool.
*   **Feature Depth**: Added detailed sections on Phase 5 AI Optimization and Phase 1 Security, which were previously undocumented or mentioned only as roadmap items.
*   **Technical Accuracy**: Replaced generic performance claims with specific mentions of DXGI, X25519, and EWMA predictors.
*   **Deployment**: Updated build instructions to reflect the necessity of `libsodium` and the `setup_deps.ps1` script.

## 2. List of removed outdated claims
*   **"Production-grade WAN Relay"**: Removed since the relay server code is not present; clarified as "Foundational WAN".
*   **"Global Cloud Scaling"**: Removed from the "Key Features" section as it remains an architectural roadmap rather than implemented code.
*   **"Sub-15ms E2E"**: Adjusted to "sub-20ms" to reflect more realistic production targets across various hardware.

## 3. Missing documentation gaps
*   **API Reference**: The `parsec_lite_api.h` is well-commented but lacks a formal documentation page for third-party integration.
*   **Linux Build Guide**: While `network_manager.cpp` has Linux paths, there is no guide for building a headless Linux client/host.
*   **AI Tuning**: Parameters for the `StreamEngine` and `LatencyPredictor` are currently hardcoded; documentation on tuning these for specific networks is missing.

## 4. Codebase Health Summary
The codebase is in **excellent health**. It demonstrates high-quality systems programming:
*   **Concurrency**: Extensive use of SPSC lock-free queues and atomic state machines.
*   **Robustness**: Comprehensive error handling in the session lifecycle and hardware initialization.
*   **Performance**: Zero-allocation hot paths for packet processing and GPU-direct media pipelines.
*   **Modernity**: Utilization of C++20 features and modern cryptographic standards.
