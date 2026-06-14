# Parsec-Lite Engineering Audit & Architecture Report

## 1. Module Breakdown

### Core Engine (ParsecLiteCore)
- **Responsibilities**:
  - Desktop Capture (DXGI Desktop Duplication)
  - Hardware Video Encoding (FFmpeg / NVENC / AMF / QSV)
  - Hardware Video Decoding (D3D11VA)
  - Network Management (Custom UDP Protocol, FEC, Jitter Buffer)
  - Input Injection (ViGEm for Gamepads, WinAPI for Keyboard/Mouse)
  - Session Management (Lifecycle control, Telemetry aggregation)
  - Global Configuration & Logging

### UI Application (appNexusDash)
- **Responsibilities**:
  - Modern Dashboard Interface (Qt 6 / QML)
  - Real-time Telemetry Visualization
  - System Monitoring (CPU, Memory, Uptime)
  - Persistent Settings Management (Dark Mode, Bitrate, FPS)
  - Mode Selection (Host vs Client)

### Shared Components
- **Common Protocol**: Binary packet definitions for Video, FEC, Input, and Feedback.
- **Utilities**: Lock-free queues, Safe queues, Profiler, Logger, and Config system.

## 2. Communication Method
- **Method**: Shared Library (DLL) / C-API.
- **Details**: `appNexusDash` links against `ParsecLiteCore.dll` and communicates via a stable C-style API defined in `parsec_lite_api.h`. The UI drives the engine state by calling API functions and pulls telemetry on a timer.

## 3. Dependency Graph

### Compile-time Dependencies
- **appNexusDash** → `ParsecLiteCore`
- **appNexusDash** → `Qt6` (Core, Gui, Qml, Quick, QuickControls2)
- **ParsecLiteCore** → `FFmpeg` (libavcodec, libavutil, libswscale)
- **ParsecLiteCore** → `ViGEmClient`
- **ParsecLiteCore** → `Windows SDK` (D3D11, DXGI, XInput, Win32)

### Runtime Dependencies
- **appNexusDash.exe** requires `ParsecLiteCore.dll`, `Qt6*.dll`, `avcodec-*.dll`, `avutil-*.dll`, `swscale-*.dll`, `ViGEmClient.dll`.
- **parsec-lite.exe** (CLI) requires `ParsecLiteCore.dll` and third-party DLLs.

## 4. Build Flow
1. **CMake Configuration**: Detects Qt, FFmpeg, and ViGEmClient. Sets up output directory to `dist/app`.
2. **Compilation**: Builds `ParsecLiteCore` (DLL), `parsec-lite` (CLI), and `appNexusDash` (UI).
3. **Post-Build Automation**:
   - `windeployqt` is invoked on `appNexusDash.exe` to bundle Qt plugins and DLLs.
   - FFmpeg and ViGEmClient DLLs are copied from `deps/` to `dist/app`.
   - `ParsecLiteCore.dll` is placed alongside the executables.

---

# Broken & Missing Features (Audit Results)

| Feature | Status | Correction Applied |
| :--- | :--- | :--- |
| **UI Telemetry** | Broken (Mocked) | Replaced mock data with real `Profiler` telemetry via `Parsec_GetTelemetry`. |
| **Dark Mode Persistence** | Missing | Implemented persistence using the `Config` system in `ThemeService`. |
| **Core/UI Separation** | Poor | Extracted streaming logic from CLI `main.cpp` into `SessionManager` in Core. |
| **System Metrics** | Mocked | Implemented real Memory usage tracking via `GlobalMemoryStatusEx`. |
| **Build Automation** | Missing | Redesigned CMake to produce a complete `dist/` folder automatically. |
| **Input Feedback** | Incomplete | Added profiling for input network latency and injection timing. |

---

# Final Build & Deployment

## Unified Build Command
```powershell
mkdir build; cd build
cmake .. -DBUILD_NEXUSDASH=ON -DCMAKE_PREFIX_PATH=<PATH_TO_QT>
cmake --build . --config Release
```

## Final Runtime Structure (dist/app/)
```text
dist/
└── app/
    ├── appNexusDash.exe     (Qt UI)
    ├── parsec-lite.exe      (CLI Tool)
    ├── ParsecLiteCore.dll   (Shared Core)
    ├── Qt6Core.dll          (and other Qt DLLs)
    ├── avcodec-60.dll       (and other FFmpeg DLLs)
    ├── ViGEmClient.dll
    ├── platforms/           (Qt Platform Plugins)
    ├── qml/                 (Qt QML Plugins)
    └── config.ini           (Persistent Settings)
```
