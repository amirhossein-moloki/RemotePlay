# Parsec-Lite Production Logging System

## Overview

Parsec-Lite features an asynchronous, thread-safe, production-grade logging subsystem engineered for high-throughput, low-latency remote play. The logger is isolated per process and per application role (`Host`, `Client`, `Standalone`, `App`), supporting dynamic log filtering, thread-local session ID correlation, lock-free performance logging, size-based log rotation, and secure diagnostics.

---

## Log Directory Hierarchy

Logs are isolated by role and identified by timestamp and Process ID (PID) to prevent file collisions during multi-instance execution:

```text
logs/
├── Host/
│   ├── host_2025-02-15_14-30-00_PID1234.log
│   ├── host_2025-02-15_14-30-00_PID1234.log.1
│   └── host_2025-02-15_15-00-00_PID5678.log
│
├── Client/
│   ├── client_2025-02-15_14-30-05_PID9012.log
│   └── client_2025-02-15_15-05-00_PID3456.log
│
└── App/
    └── app_2025-02-15_14-29-50_PID1100.log
```

---

## Log Formatting & Structure

Each log line adheres to a standard diagnostic format:

```text
[YYYY-MM-DD HH:MM:SS.mmm] [PID:<pid>] [TID:<tid>] [Session:<session_id>] [<module>] [<level>] <filename>:<line> <function>() <message>
```

### Format Fields
1. **Timestamp**: Local time with millisecond precision (`YYYY-MM-DD HH:MM:SS.mmm`).
2. **PID**: Operating system Process ID.
3. **TID**: Operating system Thread ID.
4. **Session ID**: 8-character hex string representing the active session (e.g. `A1B2C3D4`), set via thread-local context.
5. **Module**: Logical component emitting the log (e.g., `Session`, `UI_Host`, `UI_Client`, `StreamTrace`, `Decoder`).
6. **Level**: Severity level (`TRACE`, `DEBUG`, `INFO`, `WARN`, `CRIT`, `FATAL`).
7. **Source Location**: File name, line number, and function name.
8. **Message**: Log contents.

---

## Log Levels & Filtering

The subsystem supports standard log levels:

| Level | Value | Description |
|-------|-------|-------------|
| `LL_TRACE` | 0 | Detailed low-level diagnostics (e.g., packet reassembly) |
| `LL_DEBUG` | 1 | Debugging info useful during development |
| `LL_INFO`  | 2 | Normal operational events (e.g., state changes, user actions) |
| `LL_WARN`  | 3 | Non-critical warnings (e.g., packet loss, frame drop) |
| `LL_ERROR` | 4 | Errors that prevent specific actions |
| `LL_CRITICAL` | 5 | Critical subsystem failures (flushed synchronously) |
| `LL_FATAL` | 6 | Unrecoverable failures causing crash reports |

### Filtering Mechanisms
- **Compile-time Limit (`LOG_LEVEL_LIMIT`)**: Set to `LogLevel::LL_INFO` in Release builds to eliminate trace overhead.
- **Runtime Filtering**: Configured dynamically in `config.ini` via `log_level=INFO`.
- **High-Frequency Filtering**: High-frequency streaming trace modules (`StreamTrace`, `ClientTrace`) are filtered at levels below `WARN` during active sessions to preserve real-time performance.

---

## Configuration (`config.ini`)

Logger options can be adjusted in `config.ini`:

```ini
[Logging]
log_level=INFO
max_log_file_size_mb=100
max_log_files=5
enable_performance_logging=false
```

- **`log_level`**: Controls minimum severity level (`TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `CRITICAL`).
- **`max_log_file_size_mb`**: Maximum file size in megabytes before size-based rotation occurs.
- **`max_log_files`**: Maximum number of rotated log archives preserved per PID.
- **`enable_performance_logging`**: Toggles periodic high-resolution performance metrics logging.

---

## Log Rotation & Retention

When the current log file size exceeds `max_log_file_size_mb`, `Logger::rotateFiles()`:
1. Closes the current active file handle under `m_fileMutex`.
2. Deletes expired log archives exceeding `max_log_files`.
3. Renames previous log archives: `file.N` -> `file.(N+1)`.
4. Renames active log file: `base.log` -> `base.log.1`.
5. Re-opens a fresh `base.log` and writes a new configuration snapshot header.

---

## Multi-Instance & Session Correlation

- **Process Isolation**: Each instance generates a log filename containing `_PID<pid>`, preventing write contention or file overwrites.
- **Session ID Propagation**: Session IDs are tracked via thread-local storage (`t_threadSessionId`). Worker threads in `SessionManager` (e.g., packetizer, network receiver, client decoder loop) set `Logger::getInstance().setThreadSessionId(sessionId)` to correlate asynchronous streaming events across thread boundaries.

---

## Security & Privacy Guidelines

To ensure strict production compliance:
- **NEVER Log**: Encryption keys (`txKey`, `rxKey`), private key pairs, shared secrets, session tokens, passwords, or raw credentials.
- **Network Data**: Log only interface IP addresses, ports, and high-level protocol state transitions.

---

## Developer Usage Examples

### Standard Logging
```cpp
#include "common/logger.hpp"

// Operational log
LOG_INFO("UI_Session", "User initiated client connection to host 192.168.1.10");

// Warning log
LOG_WARN("Network", "Packet drop rate exceeded 2.5%");

// Error log
LOG_ERROR("Decoder", "Hardware decoder initialization failed (code: 0x887A0005)");
```

### Setting Thread-Local Session ID
```cpp
// Set active session ID for worker thread
Logger::setThreadSessionId(sessionId);

// Log event correlated with session
LOG_INFO("Session", "Keyframe reassembly completed");
```
