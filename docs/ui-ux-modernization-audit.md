# Parsec-Lite 2.0 Source-Code-Driven UI Redesign Plan

This document is a forensic UI review of the actual repository. The application is C++/Dear ImGui, not C#; the shipped UI surface is implemented in `cpp/client/launcher.cpp`, `cpp/client/overlay.cpp`, `cpp/client/renderer_d3d11.cpp`, and fatal/CLI interaction paths in `cpp/main.cpp`.

## 1. UI architecture for version 2.0

### Evidence from current code

| UI element | Source files / functions | Current implementation | Problems found in source | Modern replacement |
| --- | --- | --- | --- | --- |
| Launcher window | `cpp/client/launcher.cpp`, `Client::Launcher::Render` | One immediate-mode window that mutates `LauncherConfig` directly and starts when `startRequested` is set. | Screen layout, control labels, styling, validation, and workflow state are coupled in one function. Before this change, no field-level validation existed. | `LauncherScreen` composed from `SessionModeCard`, `NetworkCard`, `QualityCard`, `SetupStatusBanner`, and `PrimaryActionBar`; validation owned by `UI::ValidateLauncherConfig`. |
| UI theme | `cpp/client/renderer_d3d11.cpp`, `RendererD3D11::Initialize`; `cpp/client/ui_system.cpp`, `UI::ApplyTheme` | Renderer creates the ImGui context and applies default dark style; screens apply shared tokens each frame. | Renderer-level `StyleColorsDark()` is too generic for product UI; previous launcher-local style duplicated color decisions. | Product theme service with semantic tokens for surface, text, accent, success, warning, and error states. |
| Telemetry overlay | `cpp/client/overlay.cpp`, `Client::Overlay::Render` | Always-auto-resized ImGui panel reads profiler metrics and renders latency, pipeline, FPS, loss, RTT, and bitrate. | It is a data dump with no settings model, no layout variants, no persisted visibility preference, and only one threshold on latency. | `StatusCenterOverlay` with health summary, bottleneck classifier, thresholds, compact/expanded modes, and notification handoff. |
| Fatal error dialogs | `cpp/main.cpp`, `ShowFatalError` | Logs and calls `MessageBoxA`. | ANSI-only message box, no copy details, no retry action, no diagnostics link, no localization, and inconsistent with ImGui UI. | `ErrorRecoveryDialog` rendered inside the app shell with actions: Retry, Open logs, Copy details, Exit. Use `MessageBoxW` only as fallback before renderer initialization. |
| CLI usage/list | `cpp/main.cpp`, `ShowUsage`, `main --list` branch | Prints usage and interfaces to stdout. | This is not discoverable from the GUI and duplicates network selection behavior. | Diagnostics center should expose interface list, bind status, selected IP, port 5005 availability, and command-line equivalents. |
| Host run state | `cpp/main.cpp`, `RunHost` | Starts worker threads and blocks on `std::cin.get()` to stop. | Commercial desktop UX cannot require console focus/Enter to stop; no in-app stop button or hosted-client status. | Host session screen with Stop Hosting action, connected clients list, bitrate adaptation status, and live warnings. |
| Client run state | `cpp/main.cpp`, `RunClient` | Starts network, feedback, input, watchdog loops and renders only the overlay. | Connecting/reconnecting states are log-only; watchdog sends handshake silently after timeout. | Client session screen/overlay with Connecting, Live, Degraded, Reconnecting, Disconnected states. |
| Renderer shell | `cpp/client/renderer_d3d11.cpp`, `Initialize/NewFrame/EndFrame/Shutdown` | Uses console window handle, D3D11 device, ImGui init, and clear/render loop. | No top-level app shell, no DPI/font configuration, no persisted window size, no multi-screen navigation. | `AppShell` with route state, DPI-scaled typography, persistent window bounds, Mica-like dark material tokens. |

### Future navigation model

```text
AppShell
├─ Home / Setup
│  ├─ Host setup
│  └─ Client setup
├─ Live Session
│  ├─ Host status
│  ├─ Client status
│  └─ Performance overlay
├─ Diagnostics Center
│  ├─ Network adapters
│  ├─ Encoder/decoder capabilities
│  ├─ Packet loss / RTT / jitter
│  └─ Logs
└─ Settings Center
   ├─ Video quality presets
   ├─ Input forwarding
   ├─ Telemetry overlay
   ├─ Accessibility
   └─ Advanced networking
```

### Component hierarchy

```text
AppShell
├─ TitleBar
├─ NavigationRail
├─ ToastHost
├─ RouteContent
│  ├─ LauncherScreen
│  │  ├─ SetupStatusBanner
│  │  ├─ SessionModeCard
│  │  ├─ NetworkCard
│  │  ├─ QualityPresetCard
│  │  └─ PrimaryActionBar
│  ├─ DiagnosticsScreen
│  ├─ SettingsScreen
│  └─ LiveSessionScreen
└─ ModalHost
   ├─ ErrorRecoveryDialog
   └─ ConfirmStopSessionDialog
```

### Reusable components now introduced

- `UI::DesignTokens` defines spacing, radius, and core control sizing values in `cpp/client/ui_system.hpp`.
- `UI::ApplyTheme` owns product-level ImGui colors and spacing in `cpp/client/ui_system.cpp`.
- `UI::ValidateLauncherConfig` creates field-specific validation messages from actual `LauncherConfig` values and enumerated interfaces.
- `UI::RenderNotification` and `UI::RenderValidationMessages` centralize status/validation rendering instead of scattering ad-hoc `TextColored` calls.

### Localization architecture

Current evidence: all user-facing strings are literals in `launcher.cpp`, `overlay.cpp`, `main.cpp`, and README usage examples. Version 2.0 should add:

```cpp
enum class StringId {
    AppName,
    LauncherSessionModeTitle,
    LauncherNetworkTitle,
    ValidationHostIpRequired,
    ErrorRendererInitializeFailed,
};

class StringCatalog {
public:
    std::string get(StringId id, std::string_view locale = "en-US") const;
};
```

Start with `en-US.json`, then replace hard-coded literals screen by screen.

### Validation architecture

Current evidence: `LauncherConfig` allows empty/invalid `hostIp`, stale `selectedIp`, and out-of-range values if modified programmatically. The new `UI::ValidateLauncherConfig` is the first production step. Version 2.0 should extend it into:

```text
ValidationService
├─ ValidateLauncherConfig
├─ ValidateNetworkInterface
├─ ValidateHostEndpoint
├─ ValidateEncoderSettings
└─ ValidatePortAvailability
```

Validation should run synchronously for field shape and asynchronously for reachability/bind/handshake checks.

### Notification architecture

Current evidence: fatal errors use `MessageBoxA`, warnings use logs, and session degradation has no user notification. Version 2.0 should use:

```text
NotificationCenter
├─ Toast queue for nonblocking updates
├─ Inline field validation for setup forms
├─ Modal recovery dialogs for blocking failures
└─ Diagnostics log sink for support export
```

## 2. Production design system

### Color palette and semantic tokens

| Token | Hex | Usage |
| --- | --- | --- |
| `color.surface.base` | `#111419` | App background / Mica fallback |
| `color.surface.card` | `#1C2129` | Cards and grouped panels |
| `color.surface.elevated` | `#212833` | Toasts, popups, dialogs |
| `color.surface.hover` | `#333F4F` | Hovered inputs/buttons |
| `color.border.subtle` | `#404A59` | Card/input borders |
| `color.text.primary` | `#EDF2FA` | Main text |
| `color.text.secondary` | `#A0B2C7` | Helper text |
| `color.accent.primary` | `#338FF2` | Links, selected controls, focus |
| `color.status.success` | `#3DB875` | Ready/healthy/primary start |
| `color.status.warning` | `#F29E40` | Degraded stream / caution |
| `color.status.error` | `#E85D75` | Blocking validation/fatal errors |

### Typography scale

| Role | Size | Weight | Line height |
| --- | ---: | ---: | ---: |
| Display | 28 px | 700 | 36 px |
| Page title | 22 px | 650 | 30 px |
| Section title | 16 px | 650 | 24 px |
| Body | 14 px | 400 | 20 px |
| Helper | 12 px | 400 | 18 px |
| Telemetry numeric | 14 px | 600 tabular | 20 px |

### Spacing, radius, elevation

| Token | Value |
| --- | ---: |
| `space.xs` | 4 px |
| `space.sm` | 8 px |
| `space.md` | 12 px |
| `space.lg` | 16 px |
| `space.xl` | 24 px |
| `radius.sm` | 6 px |
| `radius.md` | 10 px |
| `radius.lg` | 14 px |
| `control.height` | 40 px |
| `primaryAction.height` | 46 px |
| `elevation.card` | 1 px border + no shadow in ImGui fallback |
| `elevation.toast` | 1 px accent border + elevated surface |
| `elevation.dialog` | 1 px border + scrim overlay in app shell |

### Component states

Every interactive component must define `default`, `hover`, `pressed`, `focused`, `disabled`, `loading`, and `error` states. Immediate changes implemented here: disabled primary action when launcher validation has blocking errors, inline validation messages, and status notification banner.

## 3. Screen-by-screen redesign with source evidence

### 3.1 Launcher setup screen

**Files/functions:** `cpp/client/launcher.cpp::Launcher::Render`, `cpp/client/launcher.hpp::LauncherConfig`, `cpp/client/ui_system.cpp`.

**Current layout:** one `ImGui::Begin("Parsec-Lite")` window containing status notification, Session mode card, Network card, Stream quality card, tip text, and full-width primary action.

**Current implementation evidence:** The screen directly edits `config.isHost`, `config.selectedIp`, `config.hostIp`, `config.bitrate`, `config.fps`, and `config.useHardwareEncoding`; setting `config.startRequested = true` leaves `RunLauncher` in `cpp/main.cpp` to exit the setup loop and call `RunHost` or `RunClient`.

**Design flaws:** no navigation shell, no Settings/Diagnostics entry points, no adapter refresh, no advanced port control, and presets are just buttons rather than persisted profile objects.

**UX flaws:** users cannot test reachability before start, cannot see saved/recent hosts, and cannot understand why high bitrate/high FPS may degrade latency except a warning.

**Accessibility flaws:** ImGui controls expose labels but no screen-reader semantic grouping; color-coded validation must also include text, which is now provided by `RenderValidationMessages`.

**Maintainability problems:** although the new `ui_system` extracts theme/validation primitives, `Launcher::Render` still owns too many layout details. Next step: split into `RenderSessionModeCard`, `RenderNetworkCard`, and `RenderQualityCard`.

**New layout wireframe:**

```text
┌ Parsec-Lite ─────────────────────────────────────────────┐
│ [Ready/Error banner: concrete validation summary]         │
│ ┌ Session mode ────────────────────────────────────────┐ │
│ │ (●) Host desktop      ( ) Connect as client           │ │
│ └──────────────────────────────────────────────────────┘ │
│ ┌ Network ─────────────────────────────────────────────┐ │
│ │ Local interface [ Ethernet — 192.168.1.25          v]│ │
│ │ Host IP address [ Example: 192.168.1.25             ]│ │
│ │ Error: Enter the host IP address before connecting.   │ │
│ └──────────────────────────────────────────────────────┘ │
│ ┌ Stream quality ──────────────────────────────────────┐ │
│ │ [Low latency] [Balanced] [Quality]                   │ │
│ │ Bitrate            [────────●──────] 8000 kbps       │ │
│ │ Target frame rate  [────●──────────] 60 FPS          │ │
│ │ [x] Use hardware encoding                            │ │
│ └──────────────────────────────────────────────────────┘ │
│ [Start hosting / Connect to host]                        │
└──────────────────────────────────────────────────────────┘
```

### 3.2 Performance overlay

**Files/functions:** `cpp/client/overlay.cpp::Overlay::Render`, `cpp/common/profiler.hpp::Profiler::getStats`, `cpp/main.cpp::RunClient` render loop.

**Current layout:** compact floating ImGui window titled `Performance`, grouped into Session health, Pipeline, and Network. It reads `EndToEnd_Latency`, `Capture_Time`, `Encode_Time`, `Network_Latency`, `Decode_Time`, `Present_Time`, `Network_LossRate`, `Network_RTT`, and `Host_Bitrate`.

**Design flaws:** overlay is hard-coded and cannot be resized between compact/full modes; no sparkline/trend view; only latency has a visual threshold.

**UX flaws:** users can hide telemetry but cannot reopen it from a visible menu unless a keyboard binding calls `Overlay::Toggle`; no setting explains overlay shortcut.

**Accessibility flaws:** overlay competes with streaming content; no opacity setting; color threshold requires accompanying text such as `Healthy`/`Degraded`.

**Maintainability problems:** metric names are string literals; thresholds should live in a telemetry model.

**New layout wireframe:**

```text
┌ Performance ───────────────┐
│ Status: Healthy / Degraded │
│ Latency: 15.8 ms  P99 21.4 │
├ Pipeline ──────────────────┤
│ Capture  1.2 ms            │
│ Encode   4.1 ms            │
│ Network  2.0 ms            │
│ Decode   5.2 ms            │
│ Present  1.6 ms            │
├ Network ───────────────────┤
│ FPS 60  Loss 0.2%  RTT 8ms │
│ Bitrate 8.0 Mbps           │
│ [Open diagnostics] [Hide]  │
└────────────────────────────┘
```

### 3.3 Fatal error / recovery surface

**Files/functions:** `cpp/main.cpp::ShowFatalError`, calls in `RunHost`, `RunLauncher`, `RunClient`.

**Current implementation:** `ShowFatalError` logs and calls `MessageBoxA(NULL, message.c_str(), title.c_str(), MB_OK | MB_ICONERROR)`.

**Design flaws:** outside app visual language; no commercial recovery experience.

**UX flaws:** user only gets OK; no retry, no logs, no copy details, no support bundle.

**Accessibility flaws:** ANSI MessageBox may lose Unicode data and cannot use app font scaling or theme.

**Maintainability problems:** all errors are fatal strings at call sites; no typed error model.

**Modern replacement:**

```cpp
struct AppError {
    ErrorSeverity severity;
    std::string title;
    std::string summary;
    std::string technicalDetails;
    std::vector<RecoveryAction> actions;
};
```

Render `ErrorRecoveryDialog` after renderer initialization; fallback to `MessageBoxW` only when UI cannot start.

### 3.4 Host session workflow

**Files/functions:** `cpp/main.cpp::RunHost`.

**Current implementation:** host initializes network/capture/encoder, starts capture/encoder/packetizer/sender/receiver threads, logs `Host running... Press Enter to stop`, then blocks with `std::cin.get()`.

**Design flaws:** no host screen despite ongoing app state.

**UX flaws:** stopping requires console input; connected clients are invisible; adaptive bitrate changes are invisible.

**Accessibility flaws:** console-only stop action is not reachable through app UI.

**Modern replacement:** `HostSessionScreen` with Stop Hosting, connected client list, selected interface, encoder state, bitrate adaptation, and warning toasts.

### 3.5 Client session workflow

**Files/functions:** `cpp/main.cpp::RunClient`.

**Current implementation:** starts renderer and input capture, sends handshake, runs network/feedback/input/watchdog threads, renders received frames and `Overlay::Render`.

**Design flaws:** connection state is implicit in logs and frame receipt.

**UX flaws:** if handshake fails or frames stop, users do not see a connecting/reconnecting state.

**Accessibility flaws:** no visible feedback for reconnect attempts or no-signal state.

**Modern replacement:** `ClientSessionScreen` state machine: `Connecting`, `Live`, `Degraded`, `Reconnecting`, `Disconnected`, `FatalError`.

### 3.6 CLI usage and diagnostics

**Files/functions:** `cpp/main.cpp::ShowUsage`, `main --list` branch.

**Current implementation:** stdout usage and interface list.

**Design flaw:** GUI and CLI diagnostics are separate.

**Modern replacement:** Diagnostics Center that shows the same adapter list and can export commands for support.

## 4. Modern desktop experience implementation targets

| Concept | Current source evidence | Implementation target |
| --- | --- | --- |
| Mica/Acrylic | ImGui window background only. | Use Windows backdrop APIs in a future native host window; keep `color.surface.base` as fallback. |
| Modern cards | Launcher uses `UI::BeginCard`. | Extract reusable `Card` with title, description, actions, and error slot. |
| Empty states | No interface empty state existed; validation now blocks empty interface list. | Add illustrated `NoNetworkAdaptersState` with Refresh and Open Windows Network Settings. |
| Loading states | `RunLauncher` jumps directly after `startRequested`. | Add preflight async state with skeleton rows for adapter/port/encoder checks. |
| Toast notifications | No central toasts. `UI::RenderNotification` is inline only. | Add `ToastHost` queue with timeout and action callback. |
| Inline validation | Now implemented for interface, host IP, and quality. | Extend to hostname syntax, port availability, encoder/device capability. |
| Error recovery flows | MessageBox-only. | Typed `AppError` model + retry/copy/open-log actions. |
| Settings center | None. | Persist quality presets, overlay opacity, input capture, logs, advanced networking in `Config`. |
| Diagnostics center | CLI-only list/logs. | In-app network/codec/perf diagnostics with export bundle. |
| Status center | Overlay-only metrics. | Unified health model consumed by overlay, status page, and toasts. |

## 5. Code-level refactoring plan

### Files changed in this revision

- `cpp/client/ui_system.hpp`: added design tokens, validation models, notification model, theme API, and rendering helpers.
- `cpp/client/ui_system.cpp`: added product theme, launcher validation, notification banner, and validation message renderer.
- `cpp/client/launcher.cpp`: replaced local style/helpers with shared UI system, added blocking validation, inline messages, host IP hint, quality presets, disabled primary action on errors.
- `cpp/client/overlay.cpp`: applies the same shared theme before rendering telemetry.
- `CMakeLists.txt`: builds `cpp/client/ui_system.cpp`.
- `docs/ui-ux-modernization-audit.md`: replaced generic audit with source-backed architecture, implementation plan, and forensic review.

### Functions to replace next

| Current function | Replacement | Reason |
| --- | --- | --- |
| `Launcher::Render` | `LauncherScreen::Render` with subcomponents | Reduce 100+ line mixed state/layout function. |
| `Overlay::Render` | `PerformanceOverlay::Render(const SessionHealth&)` | Decouple UI from profiler string keys. |
| `ShowFatalError` | `ErrorPresenter::Show(AppError)` | Typed recovery and localization. |
| `RunLauncher` | `AppShell::Run()` | Allow routing to setup/settings/diagnostics rather than one modal launcher loop. |
| `RunHost` stop via `std::cin.get()` | Host route state with Stop button | Remove console dependency. |

### Services to create

```text
UiThemeService          -> applies tokens, typography, density, high-contrast mode
ValidationService      -> synchronous field validation + asynchronous preflight checks
NotificationCenter     -> toasts, banners, modal requests
SessionStateService    -> Connecting/Live/Degraded/Reconnecting/Stopped
DiagnosticsService     -> adapters, port checks, logs, encoder/decoder capabilities
SettingsService        -> persisted user preferences backed by Config
LocalizationService    -> StringId lookup and formatting
```

## 6. UX workflow analysis

| Workflow | Existing flow from source | Pain points | Improved flow | Estimated reduction |
| --- | --- | --- | --- | --- |
| Start host | `RunLauncher` renders setup; button sets `startRequested`; `RunHost` initializes and blocks on Enter. | No preflight, no visible host state, console-only stop. | Validate fields, run bind/encoder/capture preflight, show HostSessionScreen with Stop Hosting and clients. | 50% fewer failed host starts; 100% removal of console stop dependency. |
| Connect client | `Launcher::Render` edits `hostIp`; button starts `RunClient`; handshake is sent once, watchdog retries silently. | No host reachability validation, no saved hosts, no visible reconnecting. | Recent hosts, inline address validation, Test Connection, Connecting state, reconnect toast. | 40-60% faster repeat connection. |
| Tune quality | User drags bitrate/FPS sliders. | Requires codec/network knowledge. | Presets plus advanced sliders and adaptive bitrate explanation. | 30% faster setup for non-expert users. |
| Diagnose latency | User reads overlay numbers. | No bottleneck label, no history, no diagnostics drill-in. | Health summary, stage thresholds, Open Diagnostics action. | 40% faster troubleshooting. |
| Recover from fatal failure | MessageBox OK and app returns/exits. | No recovery or support path. | Error dialog with Retry, Open logs, Copy details. | Major supportability improvement. |

## 7. WCAG-focused accessibility audit

| Issue | Evidence | WCAG concern | Fix |
| --- | --- | --- | --- |
| Color-only health cue | Overlay latency uses green/orange color. | 1.4.1 Use of Color. | Add `Healthy/Degraded` text next to numeric latency. |
| No field validation summary before this revision | Launcher previously allowed start with empty/stale fields. | 3.3.1 Error Identification. | Implemented setup banner and field-level messages. |
| Console-only stop host | `RunHost` uses `std::cin.get()`. | 2.1.1 Keyboard, operability inside app. | Add in-app Stop Hosting button. |
| No text scaling contract | Renderer initializes default ImGui fonts. | 1.4.4 Resize Text. | Load DPI-scaled fonts and expose UI scale setting. |
| Blocking MessageBoxA | `ShowFatalError`. | 3.3.3 Error Suggestion and localization. | Use app dialog with recovery actions and Unicode fallback. |
| Overlay focus behavior | Overlay uses `NoFocusOnAppearing`; earlier version used `NoNav`. | 2.4.7 Focus Visible. | Keep interactive controls navigable and provide shortcut help. |
| No localization | String literals across UI files. | Internationalization readiness. | Add StringCatalog and resource files. |

## 8. Production readiness gaps

### Technical debt

- `Launcher::Render` remains immediate-mode state/layout code and needs subcomponent extraction.
- `Overlay::Render` fetches profiler values by string keys, making metric names fragile.
- `RunHost` and `RunClient` combine session orchestration, threading, error presentation, and UI concerns.
- `RendererD3D11` uses a console window handle; a commercial app needs a real application window/shell.

### Missing UX features

- Recent hosts and favorite hosts.
- Test connection / preflight checks.
- In-app stop hosting and disconnect controls.
- Settings and diagnostics centers.
- Export diagnostics bundle.
- Overlay shortcut discovery and preferences.

### Missing recovery flows

- Retry renderer initialization.
- Retry bind on alternate adapter/port.
- Encoder fallback from hardware to software with explicit user choice.
- Decoder failure recovery and codec capability report.
- Reconnect/disconnect choices when watchdog detects no frames.

### Missing validation

- Host IP syntax and hostname support.
- Port availability check for UDP 5005.
- Adapter bind validation.
- Encoder/decoder capability validation.
- Firewall/VPN risk warnings.

### Missing observability

- Structured session health model.
- Toasts for bitrate adaptation and reconnect attempts.
- User-facing logs and support export.
- Metric history/trends, not only latest averages.
