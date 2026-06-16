# Error Handling Analysis Report - NexusDash

## 1. Executive Summary
The NexusDash application currently lacks a robust, centralized error handling system. Errors are primarily handled via logging to a file and a UI log table, which is insufficient for a professional user experience. Many critical failures (hardware initialization, network binding, handshake timeouts) occur silently or result in technical log entries that the average user cannot interpret.

## 2. Current Architecture & Error Flow

### 2.1 Backend (C++)
- **Core Logic:** Located in `cpp/common/session_manager.cpp`. Uses multiple threads for capture, encoding, and networking.
- **Current Error Handling:** Uses `LOG_ERROR` macros. If a critical failure occurs (e.g., `net.Bind` fails), the thread often returns or enters a sleep loop without notifying the UI.
- **API Surface:** `parsec_lite_api.h` uses `void` returns for many operations, making immediate error detection impossible for the caller.

### 2.2 Service Layer (Qt/C++)
- **SystemService:** Manages session state. It sets `m_isSessionActive = true` immediately when starting, without waiting for confirmation from the core engine.
- **Synchronization:** Uses `QMetaObject::invokeMethod` for some callbacks (connection requests), but lacks a general error notification bridge.

### 2.3 Frontend (QML)
- **UI Components:** Custom components like `NexusDialog` exist but aren't utilized for error reporting.
- **Validation:** Minimal validation on user inputs (IP addresses, bitrate, etc.).

## 3. Identified Critical Gaps

| Area | Issue | Impact |
| :--- | :--- | :--- |
| **Startup** | Network interface binding failures are silent. | User thinks hosting started but it's unreachable. |
| **Hardware** | GPU encoder/capture init failures are silent. | Black screen or no stream without explanation. |
| **Handshake** | Timeout or rejection only appears in logs. | User stuck on "Connecting..." indefinitely. |
| **Input** | No validation for IP or numeric fields. | Potential crashes or undefined behavior in C++ core. |
| **UX** | Technical logs shown to users in the Dashboard. | Poor UX; exposes system internals. |

## 4. Proposed Solution Architecture

### 4.1 Standardized Error Classification
Introduce an `AppError` struct/enum:
- `ValidationError`: Input issues.
- `NetworkError`: Bind failures, connection loss.
- `HardwareError`: GPU/Driver issues.
- `SessionError`: Handshake timeout, rejection.
- `UnexpectedError`: Crashes or unhandled exceptions.

### 4.2 Centralized Error Bridge
- **C++:** Add an `ErrorCallback` to `SessionManager` and `Parsec_API`.
- **Qt:** `SystemService` will catch these errors and emit an `errorOccurred(QString title, QString message, int category)` signal.

### 4.3 UI Implementation
- **Global Error Overlay:** A toast or notification system in `main.qml` to show transient errors.
- **Enhanced Dialogs:** Use `NexusDialog` for critical, blocking errors (e.g., "GPU not supported").
- **Field Validation:** Implement real-time validation in `NexusInput`.

## 5. User Experience Improvements
- All technical strings (e.g., `DXGI_ERROR_DEVICE_REMOVED`) will be mapped to user-friendly messages.
- Persian (Farsi) localization support for error messages as requested.
- "Retry" or "Guide" actions for common errors.
