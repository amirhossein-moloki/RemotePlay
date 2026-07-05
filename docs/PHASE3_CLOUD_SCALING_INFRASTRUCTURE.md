# Phase 3: Global Cloud Scaling & Kubernetes Infrastructure

## 1. Global Cloud Architecture Overview
The Parsec-Lite Global Infrastructure is designed as a multi-tier, edge-first distributed system supporting millions of concurrent users. It strictly separates the **Control Plane** (Signaling, Auth, Orchestration) from the **Data Plane** (Media Relays, GPU Streaming Hosts).

### Global Topology
- **Tier 1: Global Core (Control Plane Hub)**
  - Centralized Global User Registry and Session Database (e.g., CockroachDB for global consistency).
  - Global Orchestrator for inter-region resource allocation.
- **Tier 2: Regional Clusters (Local Control Plane)**
  - Regional Signaling Servers (WebSockets) for NAT traversal.
  - Regional Session Managers tracking local host/relay availability.
- **Tier 3: Edge Layer (Data Plane)**
  - **Edge Relays**: Low-latency UDP proxies (XChaCha20-blind).
  - **GPU Streaming Hosts**: Kubernetes-managed GPU instances running the host engine.

### Request Flow
1. **Client → Global DNS**: Client resolves `api.parsec-lite.com` via Geo-DNS to the nearest Regional Cluster.
2. **Client → Regional Orchestrator**: Authenticates and requests a session.
3. **Orchestrator → Session Manager**: Selects the optimal GPU Host (or Client's own host) and assigned Edge Relay.
4. **Signaling Server**: Facilitates candidate exchange (P2P Discovery) between Client and Host.
5. **Client ↔ Host (Data Plane)**: Establishes UDP stream via P2P or Edge Relay.

---

## 2. Service Breakdown (Microservices Design)
- **Signaling Service (C++/Go)**: Manages real-time WebSocket connections for SDP/ICE candidate exchange. Stateless, using Redis for pub/sub.
- **Global Orchestrator (Go/Python)**: High-level business logic, user management, and cross-region load balancing.
- **Session Manager (Go)**: Tracks the real-time state of active sessions, RTT telemetry, and node health within a region.
- **Edge Relay Node (C++)**: High-performance UDP forwarder. Performs zero-copy routing based on `RelayHeader`.
- **Host Agent (Rust/Go)**: Runs on GPU nodes to monitor `parsec-lite --host` process, GPU thermals, and encoder health.
- **Telemetry Ingestor**: Massively parallel ingestion of OpenTelemetry spans and Prometheus metrics.

---

## 3. Kubernetes Deployment Architecture
The infrastructure is orchestrated via Kubernetes (K8s) using specialized node pools and scheduling constraints.

### Cluster Architecture
- **Control Plane Pool**: Standard CPU-optimized nodes for Signaling, Orchestrator, and API Gateway.
- **GPU Host Pool**: GPU-accelerated nodes (e.g., AWS G5, Azure N-Series).
  - **Taints**: `dedicated=gpu-host:NoSchedule` to prevent standard pods from stealing GPU resources.
  - **Device Plugin**: NVIDIA Container Toolkit for pass-through to containers.
- **Edge Relay Pool**: Network-optimized instances (high PPS/bandwidth) located in Edge locations (AWS Local Zones).

### Scheduling Logic
- **Node Affinity**: `topology.kubernetes.io/region` and `zone` used to ensure Signaling and Relays are co-located with the user's geographic cluster.
- **Pod Anti-Affinity**: Ensures Signaling pods are spread across multiple availability zones for high availability.

---

## 4. Global Load Balancing Strategy
### Geo-DNS & Anycast
- **Geo-DNS**: Routes initial API traffic based on the client's source IP to the nearest Regional API Gateway.
- **Anycast IP**: Relays use Anycast (where supported) or latency-based DNS records to provide the single lowest-latency entry point for UDP traffic.

### Latency-Based Selection
- The Orchestrator uses a "Ping Mesh" to maintain a latency map between regions and edge locations.
- **Sticky Sessions**: Once a session is established, the `SessionID` is pinned to a specific Regional Cluster's Redis store to ensure signaling continuity.

### Failover
- If a region's health check fails (latency > 200ms or 50% error rate), the Global Load Balancer automatically diverts new traffic to the next-closest healthy region.

---

## 5. Auto-Scaling & Resource Management
### Scaling Rules
- **Signaling/API**: Horizontal Pod Autoscaler (HPA) based on CPU utilization and active WebSocket connection count.
- **Edge Relays**: KEDA (Kubernetes Event-Driven Autoscaling) based on aggregate egress bandwidth (e.g., scale up at 700Mbps per node).
- **GPU Hosts**:
  - **Active Session Tracking**: Scales based on the number of unassigned GPU slots.
  - **Pre-warming**: Predictive scaling based on historical usage (gaming peak hours).

### Idle Management
- **Graceful Draining**: When a GPU node is marked for scale-down, it enters `DRAINING` mode—allowing existing sessions to finish but accepting no new ones.
- **Spot Instance Integration**: Non-critical relays use Spot instances with automated failover to On-Demand instances upon interruption.

---

## 6. Observability & Monitoring Stack
- **Metrics**: Prometheus with Grafana dashboards.
  - Per-Session: FPS, RTT, Jitter, Packet Loss, Bitrate.
  - Infra: GPU Load, VRAM usage, Encode Latency, PPS (Packets Per Second).
- **Logging**: FluentBit → OpenSearch/Elasticsearch. Structured JSON logs for all services.
- **Tracing**: OpenTelemetry (OTel) for distributed tracing of the handshake process across microservices.
- **Alerting**: Alertmanager integration with PagerDuty for "Region Down" or "Relay Capacity Exhausted" events.

---

## 7. Fault Tolerance & High Availability
- **Region-Level Failure**: Multi-region active-active deployment. All session metadata is replicated using a globally distributed database.
- **Node Crash Recovery**: If a GPU node fails, the Session Manager detects the heartbeat loss and triggers a "Session Resume" event, allowing the client to reconnect to a new host within 5 seconds.
- **N+1 Redundancy**: Every regional cluster maintains a minimum of 20% over-provisioned capacity.
- **Self-Healing**: K8s Liveness/Readiness probes automatically restart hung Signaling or Relay processes.

---

## 8. Cost Optimization Strategy
- **Session Packing**: The Orchestrator prioritizes filling existing GPU nodes before spinning up new ones to maximize hardware utilization.
- **Bandwidth Management**:
  - P2P Preference: Signaling aggressively attempts UDP hole-punching to avoid Relay bandwidth costs.
  - CDN Integration: Static UI assets served via CloudFront/Cloudflare to reduce egress costs.
- **Dynamic Bitrate Scaling**: ABR (Adaptive Bitrate) logic scales down bitrate during peak pricing hours or high-congestion events to save bandwidth.

---

## 9. Security Architecture at Scale
- **mTLS**: All internal service-to-service communication (Signaling ↔ Orchestrator ↔ Relay) is secured via mutual TLS using a private CA (e.g., HashiCorp Vault).
- **Edge DDoS Protection**: UDP traffic is filtered via AWS Shield or Cloudflare Magic Transit to mitigate volumetric attacks.
- **End-to-End Encryption (E2EE)**: Media remains encrypted with XChaCha20-Poly1305 session keys. Relay nodes never see decrypted data.
- **JWT Authentication**: All Control Plane requests require a short-lived JWT signed by the Global Auth Service.

---

## 10. Step-by-Step Implementation Roadmap
1. **Milestone 1: Containerization & Local K8s**: Dockerize C++ Core, Relay, and Signaling services. Deploy to a local Minikube/Kind cluster.
2. **Milestone 2: Regional Cluster Deployment**: Deploy to AWS/Azure across two regions (e.g., US-East, EU-West) with shared Redis.
3. **Milestone 3: Global Load Balancing**: Implement Route53 Geo-DNS and Anycast routing for Relays.
4. **Milestone 4: Predictive Auto-Scaling**: Integrate KEDA with Prometheus metrics for GPU node pre-warming.
5. **Milestone 5: Hardened Observability**: Deploy full OTel and Grafana stack with real-time alerting for global outages.
