# Implementation Plan: LAN Game Streaming System

This plan outlines the development phases from a basic prototype to a production-grade system.

## Phase 1: MVP (Minimum Viable Product) - CURRENT
- **Goal**: Functional end-to-end video streaming and input forwarding.
- **Capture**: `mss` (Python).
- **Encoding**: Software H.264 (PyAV/FFmpeg).
- **Networking**: Basic UDP packetization with sequence numbers.
- **Input**: Keyboard and mouse forwarding via `pynput`.
- **Target**: Functional local network play for non-competitive games.

## Phase 2: Latency & Performance Optimization
- **Goal**: Achieve <50ms end-to-end latency.
- **Hardware Encoding**: Enable NVENC/AMF/QuickSync in the encoding pipeline.
- **Jitter Buffer**: Implement a client-side buffer to handle network jitter.
- **Frame Pacing**: Implement host-side frame pacing to match monitor refresh rate.
- **Input Optimization**: Move to lower-level input capture/injection.

## Phase 3: Robustness & Features
- **Goal**: Production-ready stability and features.
- **Adaptive Bitrate**: Implement a congestion control algorithm to adjust bitrate based on network RTT and packet loss.
- **Multi-Client Support**: Support for multiple remote players (split-screen).
- **Gamepad Support**: Implement virtual gamepad emulation using ViGEmBus.
- **UI/UX**: Create a simple interface for discovery and connection management.

## Phase 4: Native Implementation (C++/Rust)
- **Goal**: Maximum performance and resource efficiency.
- **Capture**: Move to Windows Desktop Duplication API.
- **Rendering**: Implement DirectX/Vulkan renderer for zero-copy path from decoder to screen.
- **Networking**: Highly optimized custom UDP protocol or ENet.
