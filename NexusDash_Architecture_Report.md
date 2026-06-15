# NexusDash Architecture Report

## 1. Application Overview
NexusDash is a modern desktop application for remote streaming, serving as a GUI frontend for the ParsecLite C++ streaming core. It is built using Qt 6 and QML for the presentation layer, with a C++ backend for services and core logic.

## 2. Core Modules & Architecture
### 2.1 Backend (C++ Services)
- **AppEngine**: The central controller and QML bridge. It acts as the root context object (`backend`) for the QML environment.
- **SystemService**: Manages application-level logic:
    - **Session Management**: Interfaces with `ParsecLiteCore` to start/stop host and client sessions.
    - **Network Discovery**: Uses `NetworkManager` to enumerate available IP addresses for hosting.
    - **Telemetry**: Periodically fetches performance metrics (FPS, Latency, Bitrate) from the core.
    - **System Metrics**: Monitors CPU and Memory usage (currently uses basic WinAPI/simulated data).
- **ThemeService**: Controls application-wide styling (Dark/Light mode).

### 2.2 Core Streaming Layer (ParsecLiteCore)
- **SessionManager**: The engine room. Orchestrates capture, encoding, networking, and rendering.
- **NetworkManager**: A UDP-based networking utility with support for interface enumeration and high-performance packet handling.
- **Logger**: A thread-safe logging utility that outputs to both console and `parsec-lite-core.log`.
- **ParsecLite API**: A C-style API (`parsec_lite_api.h`) that decouples the UI from the streaming implementation.

### 2.3 Presentation Layer (QML)
- **Main Layout**: A standard dashboard layout featuring a `Sidebar` for navigation and a `StackLayout` for content switching.
- **Pages**:
    - `DashboardPage`: The primary interface for launching sessions and monitoring status.
    - `SettingsPage`: Configuration for UI and streaming defaults.
    - `AboutPage`: Application information.
- **Components**: A set of custom-designed widgets (`NexusCard`, `NexusButton`, `NexusTable`, etc.) that implement the "Nexus" design language.
- **Theme Singleton**: `Theme.qml` centralizes all design tokens (colors, spacing, typography).

## 3. Dependency Chains
1. **GUI** (`NexusDash`) -> **AppEngine** -> **Services** (`SystemService`, `ThemeService`).
2. **Services** -> **ParsecLiteCore API** (`parsec_lite_api.h`).
3. **ParsecLiteCore API** -> **Internal Core Modules** (`SessionManager`, `NetworkManager`, `Logger`).

## 4. Current State Assessment
The application has a solid foundation with a clear separation of concerns. The refactoring effort will focus on:
- Elevating the QML presentation layer to commercial standards.
- Strengthening the bridge between the C++ `Logger` and the UI logs table.
- Improving the granularity of network interface data provided to the UI.
- Enhancing the visual feedback for session states (Hosting/Connected/Disconnected).
