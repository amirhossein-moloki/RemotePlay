# Phase 3: WAN Enablement & Global Connectivity Architecture

## 1. Global System Architecture (LAN → WAN Transformation)

The transition from a LAN-based system to a WAN-enabled cloud gaming platform involves a shift from direct IP discovery to an **Edge-First Architecture**. In the legacy LAN model, the client directly connected to a known host IP. In the WAN model, we introduce a decentralized infrastructure consisting of:

- **Signaling Layer**: Facilitates session matchmaking and NAT candidate exchange.
- **Traversal Layer (STUN/ICE)**: Enables peer-to-peer (P2P) connectivity through Network Address Translators (NATs) and Firewalls.
- **Relay Layer (Edge Nodes)**: Provides a high-performance fallback path (TURN-like) for cases where symmetric NATs prevent direct P2P connections.
- **Global Orchestrator**: Manages session metadata and routes clients to the nearest available Edge Relay.

### High-Level Component Interaction
1. **Host** registers with the **Global Orchestrator** and maintains a persistent signaling connection.
2. **Client** requests a session via the **Signaling Server**.
3. Both peers perform **Candidate Discovery** (STUN) and exchange their endpoints via Signaling.
4. Peers attempt **UDP Hole Punching** for a direct P2P path.
5. If direct connectivity fails, traffic is seamlessly routed through the optimal **Edge Relay Node**.

---

## 2. NAT Traversal Design (STUN/TURN/ICE flow)

Our custom NAT traversal implementation follows a simplified ICE (Interactive Connectivity Establishment) methodology optimized for sub-10ms overhead during establishment.

### Candidate Discovery
Peers gather three types of candidates:
- **Host Candidate**: Local interface IP/Port.
- **Server Reflexive Candidate (Srflx)**: Public-facing IP/Port discovered via STUN (Binding Request).
- **Relay Candidate**: IP/Port of the assigned Edge Relay node.

### The Hole Punching State Machine
1. **Phase 1: Concurrent Probing**: The client sends UDP "Hole Punch" packets to all host candidates simultaneously.
2. **Phase 2: Reflexive Mapping**: If reflexive candidates are exchanged, both peers send packets to each other's discovered public ports to "open" the NAT mapping.
3. **Phase 3: NAT Consistency Check**: The system detects NAT behavior (Endpoint-Independent vs. Address-Dependent). If symmetric NAT is detected on both ends, the system immediately switches to the Relay path.

---

## 3. Signaling Server Architecture

The Signaling Server is a low-latency coordination hub built for horizontal scalability.

- **Protocol**: Secure WebSockets (WSS) or persistent TCP with TLS 1.3.
- **Stateless Design**: The signaling server does not store session state in memory. It uses a distributed cache (e.g., Redis) for matchmaking and session routing, allowing any signaling node to handle any peer request.
- **Matchmaking**: Facilitates the "Handshake Handoff" from Phase 1 by exchanging X25519 public keys and session IDs before the UDP stream begins.
- **Load Balancing**: Region-aware DNS (GSLB) routes peers to the signaling cluster with the lowest RTT.

---

## 4. Relay / Edge Network Design

The Relay system acts as a high-bandwidth, low-latency UDP proxy.

- **Deployment**: Edge nodes are deployed in major cloud regions (AWS Local Zones, Equinix Metal) to ensure a relay hop rarely exceeds 20ms of additional RTT.
- **Encapsulation**: Media packets are wrapped in a `RelayHeader` containing the `SessionID` and `TargetUUID`. The relay performs zero-copy forwarding to minimize CPU jitter.
- **Cost vs. Performance**: Direct P2P is always preferred (zero cost). Relays are utilized only when necessary, using a "Weighted Cost" algorithm that prioritizes region-local relays over cross-continental paths.

---

## 5. Connection Routing Intelligence

The Routing Engine determines the "Best Path" using a multi-metric scoring system:

### Decision Matrix
| Path Type | Priority | Selection Criteria |
| :--- | :--- | :--- |
| **Direct (LAN)** | 1 | Same subnet detected OR RTT < 2ms |
| **P2P (WAN)** | 2 | Successful Hole Punch AND Packet Loss < 1% |
| **Relay (Edge)** | 3 | P2P failed OR P2P Packet Loss > 5% OR P2P Jitter > 20ms |

### Dynamic Path Switching
The `SessionManager` continuously monitors telemetry. If a P2P path degrades significantly during a session, the system can perform a **Live Migration** to the Relay path without dropping the session ID or cryptographic context.

---

## 6. Connection Lifecycle (Step-by-Step)

1. **Initialization**: Peers initialize `NetworkManager` and perform STUN binding.
2. **Signaling**: Client sends `SessionRequest` to Signaling Server.
3. **Candidate Exchange**: Host sends `CandidatePacket` containing Srflx and Host candidates via Signaling.
4. **Hole Punching**: Both ends begin sending UDP probes.
5. **Security Handshake**: Once a path is verified, the X25519 Handshake (Phase 1) occurs over the established UDP path.
6. **Streaming**: Media flows (Phase 2).
7. **Maintenance**: Telemetry-driven path optimization.

---

## 7. Failure Handling & Recovery Strategy

- **Hole Punch Timeout**: If P2P isn't established within 3 seconds, the client initiates the Relay fallback.
- **Session Resume**: If the UDP path is completely lost, the client re-enters the Signaling phase. The Global Orchestrator allows session resumption using the existing Phase 1 `SessionID` and rotating the XChaCha20 keys to prevent replay attacks.
- **Partial Loss**: FEC (Phase 2) is dynamically scaled up when RTT increases or when transitioning to a Relay path.

---

## 8. Scalability Model

- **Multi-Region Orchestration**: A central API manages "Sectors." Each Sector has its own Signaling and Relay clusters.
- **Geo-Proximity**: Clients are assigned to the nearest Sector based on IP geolocation or initial latency pings.
- **Horizontal Scaling**: Relays are stateless; they only require a valid `SessionID` to route traffic, allowing us to spin up/down thousands of edge nodes based on demand.

---

## 9. Security Considerations in WAN

- **Encrypted Relays**: Media remains end-to-end encrypted (E2EE) with Phase 1 session keys. The Relay nodes **cannot** decrypt the media; they only see the `SecureHeader` and `SessionID`.
- **IP Spoofing Protection**: `NetworkManager` validates that incoming packets on the P2P path match the authenticated reflexive candidates.
- **Signaling Integrity**: All signaling messages are signed using the peers' long-term identity keys to prevent session hijacking.

---

## 10. Implementation Guidance

- **NetworkManager**: Needs a non-blocking `GetPublicEndpoint` and a way to handle multiple "Candidate" sockets or mappings.
- **SessionManager**: Requires a `ConnectivityState` state machine to manage the transition from "Searching" to "Connected".
- **Protocol**: New packet types `0x0D` (CandidateDiscovery) and `0x0E` (RelayData) must be added while maintaining alignment with the `SecureHeader`.
