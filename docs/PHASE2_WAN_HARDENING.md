# Phase 2: WAN Enablement & Real Internet Hardening Architecture

## 1. Global WAN Architecture Overview
The Parsec-Lite WAN Architecture transforms the system from a LAN-only engine into a distributed, edge-first remote play platform. The system transitions from direct IP-based connectivity to a multi-layered discovery and routing model designed to overcome the complexities of the public internet (NAT, CGNAT, high jitter, and packet loss).

The architecture is composed of four primary planes:
- **Discovery Plane (STUN/ICE)**: Identifies public endpoints and NAT characteristics.
- **Signaling Plane (WSS/Orchestrator)**: Coordinates session metadata and candidate exchange.
- **Data Plane (UDP/Relay)**: Handles the high-speed media stream via P2P or Edge Relays.
- **Intelligence Plane (AI Routing)**: Dynamically selects and maintains the optimal network path.

---

## 2. NAT Traversal System Design
To achieve reliable P2P connectivity, the system implements a hardened NAT Traversal stack following a custom ICE-like methodology.

### Discovery & Probing
- **STUN Integration**: Peers query multiple global STUN servers (e.g., Google, Cloudflare) to discover reflexive (public) IP/Port mappings.
- **NAT Type Detection**: The system identifies NAT behavior (Full Cone, Restricted, Symmetric) to predict hole-punching success.
- **UDP Hole Punching**: Simultaneous bidirectional probing across all gathered candidates (Local, Srflx) to "open" NAT mappings without a centralized broker for the data path.

### Fallback Hierarchy
1. **Direct (LAN)**: Subnet-local discovery.
2. **P2P (WAN)**: Direct UDP hole punching via reflexive candidates.
3. **P2P (Symmetric)**: Port-prediction based punching (if NAT permits).
4. **Relay (TURN-style)**: High-performance fallback through regional edge nodes.

---

## 3. Signaling Server Architecture
The Signaling Server acts as the "Switchboard" of the platform, enabling peers to find each other and negotiate security contexts before the UDP stream begins.

- **Protocol**: Secure WebSockets (WSS) with TLS 1.3 for low-latency, stateful coordination.
- **Stateless Orchestration**: Signaling nodes are stateless; session state (candidate lists, public keys) is stored in a distributed cache (Redis) for horizontal scalability.
- **Handshake Handoff**: Signaling facilitates the exchange of X25519 public keys, ensuring that by the time the first UDP packet is sent, the peers already have a pre-shared session secret.
- **Security**: Signaling messages are signed using long-term identity keys to prevent spoofing.

---

## 4. Relay / Edge Infrastructure Design
The Relay system provides a deterministic fallback path for environments where P2P is impossible (e.g., Symmetric-to-Symmetric NAT).

- **Edge Deployment**: Relay nodes are deployed in Tier 1 data centers (AWS Local Zones, Equinix) to keep the "Relay Hop" under 20ms.
- **Zero-Copy Forwarding**: Relay nodes perform lightweight `RelayHeader` inspection and zero-copy packet forwarding to minimize CPU-induced jitter.
- **Load Balancing**: Clients are routed to the geographically closest relay using Geo-DNS and real-time load telemetry from the edge nodes.

---

## 5. Routing Decision Engine
The `IntelligentRouter` (AI-driven) selects the best path based on a weighted multi-metric scoring algorithm.

### Scoring Algorithm
The score for a path is calculated as:
`Score = (LatencyScore * 0.6) + (LossScore * 0.3) + (StabilityScore * 0.1)`

- **LatencyScore**: Normalized RTT (lower is better).
- **LossScore**: Inverse of the packet loss rate.
- **StabilityScore**: Based on jitter variance over the last 5 seconds.

### Dynamic Path Switching
The engine performs **Live Path Migration**. If the P2P path's score drops below the Relay path's score for >3 seconds, the stream seamlessly migrates to the Relay without a session restart.

---

## 6. Real-World Network Failure Handling
The system is hardened against the "Hostile Internet":

- **Session Roaming**: The Host allows the Client's source IP/Port to change mid-session (e.g., WiFi to LTE transition). The session is maintained as long as the new packets can be decrypted with the existing session key.
- **Adaptive FEC/ABR**: Forward Error Correction and Bitrate are dynamically scaled based on RTT spikes or packet loss bursts (0-10%).
- **Graceful Degradation**: In extreme congestion, the system prioritizes Input and Audio packets over Video to maintain "feel" while the visual quality recovers.

---

## 7. Security Model Over WAN
Security is extended from Phase 1 primitives to the WAN environment:

- **End-to-End Encryption (E2EE)**: Media is encrypted with XChaCha20-Poly1305. Relay nodes **cannot** decrypt the media; they only see the `SessionID` in the unencrypted `RelayHeader`.
- **Identity Pinning**: Peers validate each other's identity keys via the signaling channel to prevent Man-in-the-Middle (MitM) attacks.
- **Replay Protection**: A 1024-slot sliding window bitmask prevents UDP packet replay attacks, even across path migrations.

---

## 8. Connection Lifecycle (Step-by-Step Flow)
1. **Initialization**: Peers query STUN servers for candidates.
2. **Registration**: Host registers its identity and candidates with the Signaling Server.
3. **Session Request**: Client requests a session via Signaling using the Host's UUID.
4. **Candidate Exchange**: Peers exchange Srflx, Host, and Relay candidates via the encrypted Signaling channel.
5. **Probing Phase**: Both peers simultaneously probe all candidates via UDP.
6. **Path Selection**: `IntelligentRouter` selects the optimal verified path.
7. **Secure Handshake**: X25519 key exchange occurs over the chosen UDP path (Handshake Handoff).
8. **Media Streaming**: Encrypted Audio/Video flows with real-time telemetry feedback.

---

## 9. Scalability & Deployment Strategy
- **Regional Sectors**: The world is divided into sectors. Each sector has its own Signaling cluster and Relay pool.
- **Kubernetes Orchestration**: Services are containerized and scaled based on active session counts and GPU node availability.
- **Anycast Signaling**: Clients connect to a global Anycast IP to be routed to the nearest regional Signaling cluster.

---

## 10. Production Readiness Assessment (WAN Gate)
| Criteria | Target | Status |
| :--- | :--- | :--- |
| **P2P Success Rate** | > 85% (Non-Symmetric) | 🟢 Design Ready |
| **Relay Overhead** | < 15ms RTT addition | 🟢 Design Ready |
| **Roaming Recovery** | < 2 seconds | 🟡 Hardening in Progress |
| **Security** | E2EE + Identity Pinning | 🟢 Design Ready |
| **Packet Loss Resilience**| Stable at 5% Loss | 🟢 Logic implemented |
