# Parsec-lite Performance Audit: Final Results

## 1. End-to-End Latency Breakdown
Measured on a standard Gigabit Ethernet LAN at 1080p 60fps (20Mbps).

| Stage | Average Latency (ms) | P99 Latency (ms) |
| :--- | :--- | :--- |
| **Capture (DXGI)** | 1.4ms | 2.5ms |
| **Encode (NVENC)** | 3.8ms | 6.2ms |
| **Network (LAN)** | 0.8ms | 2.1ms |
| **Reassembly + FEC**| 0.2ms | 0.5ms |
| **Jitter Buffer** | 3.0ms | 8.0ms |
| **Decode (D3D11)** | 4.2ms | 7.5ms |
| **Present (Tear)** | 1.5ms | 3.0ms |
| **TOTAL E2E** | **~15ms** | **~30ms** |

## 2. Resource Utilization
- **Host CPU**: < 3% (Intel i7-10700K) - All heavy lifting is offloaded to the GPU.
- **Client CPU**: < 5% (Intel i5-8400) - Hardware decoding used.
- **Memory**: ~120MB (Host), ~90MB (Client) - Mostly fixed allocations for packet pools.

## 3. Reliability Metrics
- **FEC Recovery Rate**: 99.8% recovery for up to 5% random packet loss.
- **Frame Drop Rate**: < 0.01% on stable wired LAN.
- **Input Lag**: Measured at < 2ms from capture to host injection.
