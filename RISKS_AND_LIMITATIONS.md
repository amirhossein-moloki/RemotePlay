# Risks and Limitations

Developing a real-time game streaming system over LAN involves several technical challenges and trade-offs.

## 1. Network Jitter and Congestion
- **Risk**: Variable network latency (jitter) can cause stuttering, and congestion can lead to packet loss.
- **Impact**: Frames may arrive late or out of order, causing visible glitches or lag spikes.
- **Mitigation**: Use UDP, implement a jitter buffer, and adaptive bitrate control.

## 2. Hardware Compatibility
- **Risk**: Different GPUs support different encoding/decoding capabilities.
- **Impact**: Some systems might fall back to slow software encoding, making the system unusable for gaming.
- **Mitigation**: Implement multiple hardware backend support (NVENC, AMF, QuickSync, VAAPI).

## 3. Input Latency
- **Risk**: The round-trip time (Client Input -> Network -> Host Injection -> Frame Capture -> Network -> Client Display) adds up.
- **Impact**: If total latency exceeds 50-100ms, "competitive" or fast-paced games become unplayable.
- **Mitigation**: Minimize every stage of the pipeline; use low-latency encoding and high-priority input handling.

## 4. OS Limitations
- **Risk**: Windows and Linux have different input injection and screen capture APIs.
- **Impact**: Porting the high-performance path across platforms is complex.
- **Mitigation**: Abstract the capture and injection layers; prioritize Windows for gaming.

## 5. Security
- **Risk**: LAN traffic is typically unencrypted in simple prototypes.
- **Impact**: Malicious actors on the same network could potentially intercept the stream or inject inputs.
- **Mitigation**: Add optional AES or ChaCha20 encryption for the data streams.
