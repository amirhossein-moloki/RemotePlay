# Latency Optimization Strategies

To achieve sub-50ms latency in this LAN streaming system, the following optimizations are implemented or recommended:

## 1. Video Encoding (Host)
- **Hardware Acceleration**: Use NVENC (NVIDIA), AMF (AMD), or QuickSync (Intel). PyAV can utilize these if FFmpeg is compiled with support.
- **Zerolatency Tuning**: Use `tune='zerolatency'` and `preset='ultrafast'` in H.264 settings. This disables B-frames and other features that introduce delay.
- **CBR (Constant Bit Rate)**: Use CBR to prevent network congestion spikes caused by large I-frames.

## 2. Networking
- **UDP Protocol**: We use UDP to avoid the head-of-line blocking and retransmission delays inherent in TCP.
- **Packet Fragmentation**: Frames are split into MTU-sized packets (approx 1400 bytes) to avoid IP fragmentation.
- **Jitter Buffer**: The client should implement a small jitter buffer to handle out-of-order packets without adding significant delay.

## 3. Video Decoding (Client)
- **Hardware Decoding**: Use GPU-based decoding on the client side.
- **Low-delay Decoding**: Configure the decoder to output frames as soon as they are parsed.

## 4. Input Handling
- **Separate Channel**: Input events are sent on a separate UDP stream or handled with high priority to ensure they reach the host as fast as possible.
- **Direct Injection**: On the host, use low-level APIs like `SendInput` (Windows) or `uinput` (Linux) to minimize injection latency.

## 5. Performance Monitoring
- **End-to-End Measurement**: Calculate the time from frame capture on the host to frame display on the client.
- **Dynamic Bitrate**: Adjust the bitrate based on observed packet loss and round-trip time (RTT).
