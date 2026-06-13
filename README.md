# LAN Game Streaming (Parsec-lite)

A lightweight, low-latency LAN-only game streaming prototype built with Python. Designed for local multiplayer gaming and emulator use (e.g., PCSX2).

## Features
- **Real-time Video Streaming**: Host captures screen, encodes with H.264, and streams over UDP.
- **Input Forwarding**: Client captures keyboard and mouse events and sends them to the host for injection.
- **Multi-client Support**: Host can stream to multiple clients simultaneously.
- **Latency Optimized**: Uses `zerolatency` tuning, UDP packets, and frame skipping for minimal delay.

## Project Structure
- `host/`: Host-side streaming and input injection logic.
- `client/`: Client-side receiver and input capture logic.
- `common/`: Shared protocol definitions and packing helpers.
- `tests/`: Unit and integration tests.
- `ARCHITECTURE.md`: High-level system design.
- `CPP_BLUEPRINT.md`: Recommendations for a production-grade C++ implementation.

## Setup
### Requirements
- Python 3.8+
- FFmpeg installed and in PATH (for PyAV)
- Windows (Primary target for input injection and capture) or Linux (for testing/development)

### Installation
```bash
pip install -r requirements.txt
```
*(Note: Create a `requirements.txt` if not present: `mss`, `av`, `numpy`, `opencv-python`, `pynput`)*

## Usage
### 1. Start the Host
On the machine running the game:
```bash
python3 host/streamer.py
# and in another terminal
python3 host/injector.py
```

### 2. Start the Client
On the remote machine:
```bash
python3 client/client.py <HOST_IP>
```

## Performance Tips
- Use a wired Ethernet connection for best results.
- Lower the host resolution to 720p or 480p to reduce encoding/decoding overhead if latency is high.
- Ensure GPU acceleration is available for FFmpeg/PyAV.

## Limitations
- This is a prototype. For production use, see `CPP_BLUEPRINT.md`.
- No audio streaming implemented yet.
- Virtual Controller support (ViGEm) requires a C++ wrapper or specific Python bindings not included in this core MVP.
