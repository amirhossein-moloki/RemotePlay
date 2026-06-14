# Design System: Parsec-Lite Modern

## 1. Foundation

### 1.1 Colors (Fluent Design Palette)
| Name | Hex | Usage |
| :--- | :--- | :--- |
| **System Accent** | `#0078D4` | Primary actions, focus states, active indicators. |
| **Background (Mica)** | `#202020` | Main window background (Dark mode). |
| **Surface (Card)** | `#2B2B2B` | Containers, cards, and elevated surfaces. |
| **Neutral Primary** | `#FFFFFF` | Primary text, headers. |
| **Neutral Secondary**| `#A1A1A1` | Secondary text, captions, inactive icons. |
| **Neutral Tertiary** | `#323232` | Borders, dividers, subtle button backgrounds. |
| **Success** | `#6BB05D` | Telemetry health, connected state. |
| **Warning** | `#FCE100` | Jitter warnings, moderate packet loss. |
| **Error** | `#E81123` | Critical errors, disconnected state, high packet loss. |

### 1.2 Typography
**Font Family:** `Segoe UI Variable`, `Segoe UI`, `System Default`

| Role | Size | Weight | Line Height |
| :--- | :--- | :--- | :--- |
| **Header 1** | 28px | Bold | 36px |
| **Header 2** | 20px | Semibold | 28px |
| **Sub-header** | 16px | Semibold | 24px |
| **Body Regular** | 14px | Normal | 20px |
| **Caption** | 12px | Normal | 16px |
| **Monospace** | 14px | Normal | 20px | (For Telemetry values)

### 1.3 Spacing & Grid
- **Grid Unit:** 4px
- **Page Padding:** 24px
- **Card Spacing:** 16px
- **Inner Spacing:** 8px or 12px
- **Corner Radius:** 4px (Buttons/Inputs), 8px (Cards/Flyouts)

---

## 2. Component Library

### 2.1 Buttons
- **Primary:** Solid `#0078D4` with White text. Hover: `#005A9E`.
- **Secondary:** Transparent with 1px `#323232` border.
- **Subtle:** No border, background appears only on hover.

### 2.2 Input Controls
- **Text Field:** 32px height, bottom border (active), light gray background.
- **Toggle Switch:** Standard Fluent toggle for "Hardware Encoding".
- **Slider:** Integrated with a numeric value label on the right.

### 2.3 Cards (The "Container")
- **Layout:** Vertical stack with Header, Body, and optional Footer.
- **Visuals:** 1px border (`Neutral Tertiary`), subtle drop shadow.
- **Usage:** Host settings, Client settings, Interface selection.

### 2.4 Navigation (Sidebar)
- **Width:** 240px (Expanded), 48px (Collapsed).
- **Items:** Icon + Label.
- **States:** Hover, Active (Accent vertical bar on left).

### 2.5 Status Indicators (InfoBadge)
- **Small circular dots:** Red/Yellow/Green.
- **Usage:** Network quality indicator in the top-right of the dashboard.

---

## 3. Telemetry Visualizations (New)

### 3.1 Performance Sparklines
- **Description:** Small, 100x40px line charts showing the last 60 seconds of latency.
- **Logic:** Green if < 20ms, Yellow if 20-50ms, Red if > 50ms.

### 3.2 Connectivity Gauge
- **Description:** Semi-circular gauge for Bitrate.
- **Logic:** Max value dynamic based on Target Bitrate setting.

---

## 4. Accessibility Specs (WCAG 2.1)
- **Contrast:** Maintain 4.5:1 ratio for all body text.
- **Focus States:** High-visibility 2px accent border around focused elements.
- **Alt Text:** Every icon button (e.g., "Settings") must have a descriptive tooltip/aria-label.
- **Scaling:** UI must support 100% to 200% DPI scaling without layout breakage.
