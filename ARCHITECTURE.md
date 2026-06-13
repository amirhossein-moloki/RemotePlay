# LAN Game Streaming System Architecture (Parsec-lite)

## Overview
This system allows for low-latency game streaming over a Local Area Network (LAN). It consists of a Host (the machine running the game) and one or more Clients (the machines receiving the stream and sending inputs).

## Architecture Diagram

```text
+---------------------+                       +---------------------+
|       HOST          |                       |       CLIENT        |
|                     |                       |                     |
|  +---------------+  |       Video (UDP)     |  +---------------+  |
|  |  Screen Cap   |-------------------------->|    Receiver   |  |
|  +---------------+  |      (Broadcast)      |  +---------------+  |
|         |           |                       |  +---------------+  |
|  +---------------+  |                       |         |           |
|  |    Encoder    |  |                       |  +---------------+  |
|  +---------------+  |                       |  |    Decoder    |  |
|         |           |                       |  +---------------+  |
|  +---------------+  |                       |         |           |
|  |   Streamer    |  |                       |  +---------------+  |
|  +---------------+  |                       |  |    Renderer   |  |
|                     |                       |  +---------------+  |
|  +---------------+  |       Input (UDP)     |  +---------------+  |
|  | Input Injector |<--------------------------| Input Capture |  |
|  +---------------+  |                       |  +---------------+  |
|                     |                       |                     |
+---------------------+                       +---------------------+
```

## Module Breakdown

### Host Modules
1. **Screen Capture**: Uses `mss` for fast cross-platform screen grabbing.
2. **Video Encoder**: Encodes frames into H.264 using `PyAV` (FFmpeg) with `zerolatency` tuning.
3. **Streaming Server**: Manages a set of connected clients and broadcasts video packets via UDP.
4. **Input Injector**: Listens for input events and uses `pynput` to simulate keyboard and mouse actions.

### Client Modules
1. **Receiver**: Reassembles fragmented UDP packets and maintains a sequence-aware buffer to handle jitter and drop stale frames.
2. **Video Decoder**: Decodes H.264 bitstream using `PyAV`.
3. **Renderer**: Displays frames using `OpenCV`'s `imshow`.
4. **Input Capture**: Uses `pynput` to capture keyboard and mouse events (clicks, scrolls, moves).

## Network Protocol
- **Video Packet**: `[Type:1][Sequence:4][FragIdx:2][TotalFrags:2][PayloadSize:2][Data...]`
- **Input Packet**: `[Type:1][InputType:1][Payload...]`
  - Input types include: Key Press/Release, Mouse Move/Click/Scroll.

## Multi-client Support
The host maintains a list of clients who have sent a `CONNECT` packet. Every encoded frame is split into UDP packets and sent to every registered client address. Stale clients are not currently pruned in this prototype but would be in a production system.

## Future Improvements
- **Audio Streaming**: Add Opus-encoded audio stream.
- **ViGEm Integration**: Support virtual Xbox controllers for better game compatibility.
- **Congestion Control**: Implement adaptive bitrate based on RTT and loss.
