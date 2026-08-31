# Parsec-Lite Support & Crash Diagnostics System

## Overview

The Parsec-Lite Support & Diagnostics subsystem provides automated, non-intrusive crash reporting and diagnostic support package generation. Designed for field operations, it enables rapid troubleshooting of production issues without compromising user security or stream latency.

---

## 1. Support Package Generator

Support diagnostic archives can be generated programmatically or via UI/API requests using `Parsec_GenerateSupportPackage(const char* outputPath)`.

### Archive Directory Layout

Packages are structured according to standard diagnostic organization:

```text
ParsecLite_Support_<timestamp>_<role>_<pid>/ (or specified zip/tar archive root)
│
├── Logs/
│   ├── Host/
│   │   └── host_2025-02-15_14-30-00_PID1234.log
│   └── Client/
│       └── client_2025-02-15_14-30-05_PID9012.log
│
├── CrashReports/
│   ├── crash_2025-02-15_14-45-12.txt
│   └── crash_2025-02-15_14-45-12.zip
│
├── Configuration/
│   └── config.ini (Sanitized & Redacted)
│
├── SystemInfo/
│   └── system_info.txt
│
├── Telemetry/
│   └── telemetry_history.txt
│
└── Network/
    └── network_diagnostics.txt
```

### Key API Guarantees
- **Non-blocking Execution**: Safe to generate while an active streaming session is running.
- **Path Traversal & Injection Safety**: Output paths are validated via `isSafePath()` to block directory traversal (`..`) and shell command injection symbols (`; | & $ < > " '`).
- **Secret Redaction**: `config.ini` is scanned upon collection, automatically redacting lines containing passwords, tokens, private keys, or credentials.
- **Graceful Diagnostics Recovery**: Missing directories, unavailable telemetry metrics, or unreadable logs are handled without throwing exceptions or crashing.

---

## 2. Crash Diagnostics Handler

The `CrashDiagnostics::CrashHandler` system automatically captures unhandled OS exceptions and POSIX signals.

### Captured Diagnostic Details
When an unexpected crash occurs, `CrashHandler::GenerateCrashReport` creates a report in `CrashReports/` containing:
1. **Exception / Signal Header**: Exception code, human-readable description, fault address, and faulting module.
2. **CPU Registers**: Full register dump (RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP on x64).
3. **Stack Backtrace**: Frame-by-frame call stack addresses and module names.
4. **Loaded Modules**: Complete listing of active process DLLs/shared libraries with base addresses.
5. **Active Threads**: Thread IDs and priority states.
6. **Active Session State**: Current Session ID, role, and engine operational state.
7. **System & GPU Memory**: Memory usage (Working Set, Pagefile, RAM limits) and DirectX/DXGI device state.

### Safety & Reliability Guarantees
- **Reentrancy Protection**: Protected by an atomic guard (`s_crashReportGenerated`) to prevent infinite recursion or thread deadlocks if secondary faults occur during crash processing.
- **Async-Signal Safety**: Skips external shell compression when executing inside POSIX signal handlers to prevent deadlocks or signal safety violations.
- **Log Integrity**: Crash reports are written to independent text files (`CrashReports/crash_<timestamp>.txt`), leaving active log streams uncorrupted.

---

## 3. Developer Usage

### Programmatic Support Package Generation
```cpp
#include "common/parsec_lite_api.h"

// Generate support diagnostic archive
bool success = Parsec_GenerateSupportPackage("support_diagnostics.zip");
if (success) {
    LOG_INFO("Support", "Support package successfully generated.");
} else {
    LOG_ERROR("Support", "Failed to generate support package.");
}
```

### Crash Handler Initialization
```cpp
#include "common/crash_handler.hpp"

// Initialized automatically on Parsec_Initialize()
CrashDiagnostics::CrashHandler::Initialize();
```
