# Parsec-lite Networking Layer Hardening

## 1. Redesigned Network Architecture

The networking layer has been refactored to use a multi-threaded, pool-based, jitter-resistant architecture.

### Host Architecture:
```text
[Capture Thread] -> [Capture Queue] -> [Encoder Thread]
                                            |
                                      [Packet Pool] (Pre-allocated)
                                            |
[Packetizer Thread] <- [Encode Queue] <-----/
      |
      |-- Fragment Video Packets
      |-- Generate XOR FEC Parity (every 5 fragments)
      |-- [UDP Packet Pool]
      |
[Sender Thread] <--- [Send Queue] (Low-lock)
      |
      UDP Socket -> [Network]
```

### Client Architecture:
```text
[UDP Socket] -> [Receiver Thread]
                      |
              [Receiver Reassembly] <--- [FEC Recovery]
                      |           \---- [Packet Pool]
                      |
              [Frame Ring Buffer] (O(1) Lookups)
                      |
              [Jitter Buffer] (Priority Reordering & Delay Smoothing)
                      |
[Render Thread] <- [Decoder Thread]
```

## 2. Optimized UDP Packet Format

### Video Packet Header (24 bytes)
| Field | Type | Description |
|---|---|---|
| `type` | `uint8_t` | `PacketType::Video` (0x01) |
| `frameId` | `uint32_t` | Monotonic frame counter |
| `fragmentIndex` | `uint16_t` | Index within the frame |
| `totalFragments` | `uint16_t` | Total frags in the frame |
| `packetSequence` | `uint32_t` | Global global UDP sequence |
| `timestamp` | `uint64_t` | Capture timestamp (us) |
| `flags` | `uint8_t` | Bit 0: Keyframe |
| `dataSize` | `uint16_t` | Payload size |

### FEC Packet Header (15 bytes)
| Field | Type | Description |
|---|---|---|
| `type` | `uint8_t` | `PacketType::FEC` (0x05) |
| `frameId` | `uint32_t` | Associated frame |
| `fragmentStart` | `uint16_t` | Group start index |
| `fragmentCount` | `uint16_t` | Fragments in group |
| `packetSequence` | `uint32_t` | Global UDP sequence |
| `dataSize` | `uint16_t` | XORed payload size |

## 3. Explanations

### Eliminating Stutter
Stutter is eliminated through three mechanisms:
1.  **Jitter Buffer**: Buffers 1-3 frames (configurable) and uses a priority queue to ensure frames are delivered in order even if they arrive out-of-order over UDP.
2.  **FEC (Forward Error Correction)**: By sending an XOR parity packet for every group of fragments, we can recover any single lost packet in that group without waiting for retransmission.
3.  **HoL Blocking Avoidance**: The `Receiver` and `JitterBuffer` are designed to skip stale frames if newer complete frames are available, ensuring the system always prioritizes the most recent data.

### Removing Memory Allocations
1.  **Packet Pools**: Both Host and Client use pre-allocated pools for large video buffers and small UDP fragments. No `new`, `malloc`, or `std::vector` resizing occurs in the hot path.
2.  **O(1) Reassembly**: Replaced `std::map` with a `FixedRingBuffer` indexed by `frameId`, allowing O(1) insertion and retrieval of frames during reassembly.
3.  **Stack-based Fragment Tracking**: The host uses thread-local fixed-size arrays to track fragments during packetization instead of dynamic vectors.

### Jitter Handling
The `JitterBuffer` uses `std::chrono` to track the "residence time" of each frame. It ensures a minimum delay (e.g., 10ms) to absorb network jitter while also allowing rapid "catch-up" if the buffer fills up, maintaining a tight balance between stability and latency.
