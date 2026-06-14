# UX Strategy & Screen-by-Screen Analysis

## 1. User Workflows

### 1.1 Host Session Flow
**Current Behavior:**
1. Open Launcher.
2. Ensure "Host Mode" is selected.
3. Select Interface.
4. Click "START SESSION".
5. App waits for client (Terminal-style).

**Problems:**
- No visual confirmation that the host is "Live" or "Waiting".
- Settings are mixed with operation modes.
- Hard to tell which IP to give the client.

**Improved Workflow:**
1. **Host Dashboard:** User sees a "Start Hosting" card.
2. **Network Auto-Detection:** App pre-selects the primary ethernet/wifi interface.
3. **One-Click Start:** Click "Go Live".
4. **Live Status:** Dashboard transforms to show "Hosting Active", displays the Host IP in large, copyable text, and lists connected clients.

### 1.2 Client Connection Flow
**Current Behavior:**
1. Open Launcher.
2. Select "Client Mode".
3. Manually type IP.
4. Click "START SESSION".

**Problems:**
- High friction (manual typing).
- No history of previous connections.
- No "Ping" or status check before connecting.

**Improved Workflow:**
1. **Connect Page:** Shows "Recent Connections" cards (Previous IPs).
2. **Ping Preview:** App automatically pings recent hosts to show "Online" status and latency.
3. **Quick Connect:** Click a "Recent" card or enter a new IP with auto-complete/masking.

---

## 2. Screen-by-Screen Redesign

### Screen 1: The Dashboard (Home)
- **Purpose:** Central hub for session control and status.
- **Proposed Layout:**
    - **Sidebar:** Home, Host, Connect, Settings, About.
    - **Main Content:**
        - Top: "System Status" banner (Green/Red).
        - Center: Two large "Action Cards" - [Be a Host] and [Connect to Host].
        - Bottom: "Performance Snapshot" from last session.

### Screen 2: Host Settings Page
- **Purpose:** Configure streaming parameters.
- **Proposed Layout:**
    - **Group 1: Video Settings:** Resolution (Dropdown), FPS (Slider + Input), Bitrate (Slider + Input).
    - **Group 2: Hardware:** Hardware Encoding Toggle, GPU Selector.
    - **Group 3: Network:** Interface Selection, Port configuration.

### Screen 3: Telemetry Overlay (In-Game)
- **Purpose:** Monitor stream health without obstruction.
- **Proposed Layout:**
    - **Mini-Bar Mode:** A thin horizontal bar at the top/bottom (Latency | FPS | Loss).
    - **Detailed View (Hot-keyable):**
        - Left: Micro-charts for Latency and Jitter.
        - Center: Frame breakdown (Capture, Encode, Network, Decode).
        - Right: Bitrate and Loss gauges.
    - **Visual Cues:** Elements glow Red if thresholds are exceeded.

---

## 3. Estimated Usability Improvement
- **Friction Reduction:** ~40% fewer clicks for repeat sessions.
- **Error Reduction:** ~60% improvement via input validation and "Recent Connections".
- **Glanceability:** ~80% improvement via color-coded telemetry and gauges.
