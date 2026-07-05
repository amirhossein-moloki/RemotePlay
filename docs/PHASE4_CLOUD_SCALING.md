# Phase 4: Cloud Scaling & Global Infrastructure

## 1. Global Cloud Architecture Overview
The Parsec-Lite Global Infrastructure is designed as a multi-tier, edge-first distributed system. It strictly separates the **Control Plane** (Orchestration, Auth, Session Management) from the **Data Plane** (Media Relays, Edge Streaming).

### Full System Topology
- **Global Core (Tier 1)**: Centralized Database (Global User/Host Registry) and Global Orchestrator.
- **Regional Clusters (Tier 2)**: Regional Signaling Servers and Session Managers.
- **Edge Layer (Tier 3)**: Distributed Edge Relays and GPU-accelerated Host Nodes (Cloud Gaming Hosts).

### Control Plane vs. Data Plane
- **Control Plane**: Handles API requests, user authentication, session negotiation, and matchmaking. Optimized for consistency and reachability.
- **Data Plane**: Handles the ultra-low latency UDP stream (XChaCha20-Poly1305 encrypted). Optimized for raw throughput and minimal RTT.

---

## 2. Service Breakdown (Microservices Design)
The system is decomposed into specialized microservices to enable independent scaling and isolation.

- **Signaling Service**: Low-latency WebSocket-based coordination for NAT traversal and candidate exchange (Phase 3).
- **Session Orchestrator**: Manages the lifecycle of sessions, performs region-selection, and tracks relay utilization.
- **Relay Server (Edge Node)**: High-performance C++ implementation for zero-copy UDP forwarding.
- **Host Manager**: Deployed on GPU nodes; manages the lifecycle of the `parsec-lite --host` process and monitors hardware health.
- **Telemetry Service**: Ingests real-time metrics from both clients and hosts for observability and ABR (Adaptive Bitrate) feedback.

---

## 3. Deployment Model (Kubernetes / Containers)
All services are containerized using Docker and orchestrated via Kubernetes (K8s) for resilience.

### Kubernetes Strategy
- **Standard Nodes**: Run Signaling and Orchestration services (General purpose instances).
- **GPU Nodes**: Specialized K8s node pools (e.g., AWS G5, Azure N-Series) using **NVIDIA Container Toolkit**.
- **Taints & Tolerations**: GPU workloads are tainted to ensure only host-related pods are scheduled on expensive hardware.
- **Pod Scaling Strategy**:
  - Horizontal Pod Autoscaler (HPA) for Signaling (based on CPU/Connection count).
  - Custom Metrics Autoscaler (KEDA) for Relays (based on bandwidth throughput).

---

## 4. Load Balancing & Traffic Routing Strategy
A multi-layered routing engine ensures users always connect to the lowest-latency node.

- **Global Load Balancer (Geo-DNS/Anycast)**: Routes the initial client request to the nearest Regional Cluster.
- **Session-Aware Routing**: The Orchestrator assigns a specific `RelayID` or `HostID` based on the client's public IP and real-time RTT probes.
- **Failover Strategy**: If a Regional Cluster becomes unresponsive, Geo-DNS shifts traffic to the next closest region. Active sessions attempt to reconnect via the "Session Resume" protocol (Phase 3).

---

## 5. Auto-Scaling Design
Scaling is driven by predictive algorithms and real-time demand.

- **Relay Scaling**: Relays scale horizontally across Edge locations. New instances spin up when aggregate bandwidth exceeds 70% of capacity.
- **GPU Node Scaling**:
  - **Pre-warming**: Predictive scaling based on historical data (e.g., peak gaming hours in specific time zones).
  - **Graceful Termination**: Nodes enter a "Draining" state where they stop accepting new sessions but allow existing ones to finish before shutting down.
- **Cost-Aware Policies**: Utilization of Spot Instances for non-critical relay capacity, with automated fallback to On-Demand instances.

---

## 6. Observability & Monitoring Stack
A comprehensive stack for real-time failure detection and performance tuning.

- **Metrics**: Prometheus & Grafana tracking FPS, RTT, Jitter, Packet Loss, and GPU Encode Latency.
- **Logging**: Centralized ELK Stack (Elasticsearch, Logstash, Kibana) for structured logs.
- **Distributed Tracing**: **OpenTelemetry** integration across Signaling and Orchestration to trace session establishment bottlenecks.
- **Alerting**: Alertmanager triggers PagerDuty/Slack for region-wide latency spikes (>50ms) or high packet loss (>5%).

---

## 7. Fault Tolerance & High Availability
Designed for "N+1" redundancy at every layer.

- **Region Failure**: Multi-region active-active deployment. All session metadata is replicated across regions via a globally distributed database (e.g., CockroachDB or DynamoDB Global Tables).
- **Node Crash Recovery**: If a Host node crashes, the Host Manager automatically restarts the process. The client detects the timeout and re-initiates the handshake via Signaling.
- **Session Migration**: (Optional/Experimental) Ability to hand off a session from one Relay to another if the network path degrades, using the Phase 3 `RelayHeader` context.

---

## 8. Cost Optimization Strategy
Maximizing performance per dollar spent on cloud infrastructure.

- **Bandwidth Optimization**: Prioritizing P2P (Phase 3) to bypass Relay costs. Relays are only used when symmetric NAT is detected.
- **Adaptive Quality**: Dynamic resolution/bitrate adjustment based on cost-per-GB thresholds during peak hours.
- **Edge Server Utilization**: Using lightweight, ARM-based edge instances (e.g., Graviton) for Relay nodes to reduce compute costs.

---

## 9. Security Architecture at Scale
Extending Phase 1 security to the global cloud environment.

- **mTLS (mutual TLS)**: All inter-service communication (Signaling to Orchestrator, etc.) is secured with mTLS.
- **DDoS Protection**: Infrastructure-level protection (e.g., AWS Shield, Cloudflare Magic Transit) to protect UDP ingest ports.
- **API Gateway Security**: JWT-based authentication for all client-to-control-plane requests.
- **E2EE (End-to-End Encryption)**: Relay nodes never possess the X25519 session keys. They remain blind proxies for the XChaCha20-Poly1305 encrypted payload.

---

## 10. Implementation Roadmap
1. **Milestone 1**: Containerize existing C++ components (Host, Client, Relay) and validate in a single-region K8s cluster.
2. **Milestone 2**: Deploy Global Signaling Cluster with Redis-backed session state.
3. **Milestone 3**: Implement Geo-DNS and multi-region Relay deployment (US-East, EU-West, Asia-East).
4. **Milestone 4**: Integrate KEDA for custom-metrics autoscaling (Bandwidth/GPU).
5. **Milestone 5**: Full Observability integration and production-grade stress testing (Phase 4 completion).
