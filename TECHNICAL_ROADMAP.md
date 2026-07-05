# Technical Roadmap & Implementation Strategy

## 1. Architecture: Hybrid C#/C++ Approach

To modernize the UI while preserving the ultra-low latency C++ streaming engine, we will use a **Hybrid Architecture** (C# WinUI 3 + C++ Core).

### 1.1 Technology Stack
- **Frontend:** WinUI 3 (Windows App SDK) / C#
- **Backend Core:** Existing C++ (DirectX 11, FFmpeg, DXGI)
- **Bridge:** **C++/WinRT** or **P/Invoke Layer**.
- **State Management:** MVVM (CommunityToolkit.Mvvm).

### 1.2 Proposed Project Structure
```text
/ParsecLite.sln
  /ParsecLite.UI (C# / WinUI 3)
    /Views
    /ViewModels
    /Services
      StreamingService.cs (Wrapper for C++ calls)
  /ParsecLite.Core (C++ / WinRT Component)
    (Existing C++ logic refactored into a DLL/WinRT component)
```

### 1.3 Bridging Strategy
1.  **Refactor `main.cpp`:** Extract the logic from `RunHost` and `RunClient` into a reusable library (`ParsecCore.dll`).
2.  **Expose API:** Create a stable C-style API or C++/WinRT projection.
    - `StartHost(Config params)`
    - `ConnectToHost(string ip, Config params)`
    - `GetTelemetry(out TelemetryStats stats)`
3.  **DirectX Sharing:** The C# frontend will handle the "Launcher" and "Dashboard" windows. When a session starts, the C++ core takes over the rendering context or provides a `SharedHandle` for the D3D11 texture to be displayed in a WinUI `SwapChainPanel`.

---

## 2. Priority Matrix

| Category | High Impact / Low Effort (Quick Wins) | High Impact / High Effort (Big Bets) |
| :--- | :--- | :--- |
| **Critical** | Input Validation (IP/Bitrate); Error Dialogs for "Device Lost". | **C# Frontend Migration**; Decoupling UI from Core thread. |
| **High** | Dark/Light mode support; Color-coded telemetry thresholds. | **Recent Connections History**; Auto-discovery of Local Hosts. |
| **Medium** | "Copy IP to Clipboard" button; Tooltips for settings. | **Visual Telemetry Charts** (Sparklines); Gauge components. |
| **Nice to Have** | Custom Iconography; UI Animations (Transitions). | **Localization** (Multi-language support). |

---

## 3. Implementation Roadmap

### Phase A: Core Refactoring (Weeks 1-2)
- Encapsulate global state into a `SessionContext`.
- Build `ParsecCore.dll` with a clean C API.
- Implement telemetry data export (Struct-based).

### Phase B: WinUI 3 Shell (Weeks 3-4)
- Set up WinUI 3 project structure.
- Implement the Navigation Sidebar and Theme Engine.
- Create the "Host" and "Connect" basic views.

### Phase C: Integration (Weeks 5-6)
- Implement P/Invoke wrappers in C#.
- Map UI settings to C++ config structs.
- Handle session lifecycle (Start/Stop/Error) from C#.

### Phase D: UX Polish & Overlay (Weeks 7-8)
- Port the ImGui overlay logic to a high-performance Win2D or D3D11 overlay managed by the core.
- Implement "Recent Connections" persistence (SQLite or JSON).
- Final Accessibility audit and focus-ring tuning.

### Phase E: Cloud Scaling & Global Infrastructure (Target: Q3/Q4)
- **Containerization**: Dockerize Core/Relay/Signaling services.
- **Orchestration**: K8s deployment with GPU-aware scheduling.
- **Global Routing**: Geo-DNS and Latency-based server selection.
- **Observability**: OpenTelemetry and Grafana integration.
