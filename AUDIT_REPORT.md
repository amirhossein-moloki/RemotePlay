# UI/UX Audit Report: Parsec-Lite

## 1. Window Layouts & Navigation
| Issue | Severity | Description | Impact | Recommended Solution |
| :--- | :--- | :--- | :--- | :--- |
| **Absolute Positioning** | Medium | Windows use `ImGuiCond_FirstUseEver` with hardcoded coordinates. | UI may be cut off on smaller screens or misplaced on multi-monitor setups. | Use relative positioning or a modern Windowing system (WinUI 3) that handles layout scaling. |
| **Floating Window Proliferation** | Medium | Launcher and Telemetry are separate floating windows with no parent container. | Increases visual clutter and makes the app feel like a "debugging tool" rather than a product. | Consolidate into a single Dashboard window with a Sidebar navigation. |
| **Lack of Responsive Design** | High | UI components use absolute pixel spacing. | Poor usability on 4K monitors or handheld devices (Steam Deck). | Implement a Flexbox or Grid-based layout system. |

## 2. Forms & Input Controls
| Issue | Severity | Description | Impact | Recommended Solution |
| :--- | :--- | :--- | :--- | :--- |
| **Missing Input Validation** | Critical | "Host IP Address" accepts any string without format checking. | App crashes or hangs if an invalid IP is entered. | Implement Regex validation and real-time "Format Error" feedback. |
| **Manual Bitrate Entry** | Medium | Bitrate uses a slider (1000-20000) with no text input. | Difficult to set specific values (e.g., exactly 8500 kbps). | Add a numeric input field alongside the slider. |
| **Implicit Mode Switching** | Medium | Radio buttons for Host/Client mode don't change the UI context drastically enough. | Users may start a session in the wrong mode. | Use a clear "Card-based" selection or dedicated tabs for Host vs. Client. |

## 3. Telemetry & Data Visualization
| Issue | Severity | Description | Impact | Recommended Solution |
| :--- | :--- | :--- | :--- | :--- |
| **Text-Heavy Overlay** | High | Telemetry is a wall of text (ms, FPS, %). | Difficult to process during high-action gameplay. | Use sparklines for latency and color-coded gauges for packet loss. |
| **Static Thresholds** | Medium | Loss and Latency don't change color based on health. | Users don't notice performance degradation until it's unplayable. | Implement "Green/Yellow/Red" state indicators (e.g., Red for >5% loss). |

## 4. Accessibility
| Issue | Severity | Description | Impact | Recommended Solution |
| :--- | :--- | :--- | :--- | :--- |
| **No Screen Reader Support** | High | Dear ImGui lacks native MSAA/UIA support. | Blind or low-vision users cannot use the application. | Migrate to WinUI 3 or WPF which provides full accessibility tree support. |
| **Poor Contrast** | Medium | Green text on dark gray (`0.3f, 0.8f, 0.2f`). | Hard to read for users with color vision deficiencies. | Follow WCAG 2.1 contrast ratios (min 4.5:1). |
| **Keyboard Navigation** | Medium | Limited tab-order control. | Power users cannot navigate efficiently. | Implement a standard focus-ring and logical Tab sequence. |

## 5. Technical Implementation
| Issue | Severity | Description | Impact | Recommended Solution |
| :--- | :--- | :--- | :--- | :--- |
| **Tight Coupling** | High | UI code directly modifies global config and triggers networking. | Difficult to test UI independently of the streaming core. | Use an MVVM (Model-View-ViewModel) pattern. |
| **UI Blocking Risk** | High | Launcher loop runs in the same thread as D3D11 initialization. | If network discovery hangs, the UI freezes. | Run UI and Core Logic on separate threads with a message queue. |
