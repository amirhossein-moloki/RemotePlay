# Parsec-Lite: UI/UX Modernization & Redesign Proposal

## 1. Executive Summary
Parsec-Lite is a high-performance LAN game streaming system with a robust C++ core. However, its current user interface, built with Dear ImGui, is tailored for developers and lacks the accessibility, visual polish, and intuitive workflows expected of a professional consumer-grade application.

This proposal outlines a complete modernization strategy. We recommend transitioning to a **Hybrid Architecture** using a **C# WinUI 3** frontend while preserving the **C++ streaming engine**. This approach allows us to leverage modern Windows UI capabilities (Mica, Acrylic, Responsive Grids) and comprehensive accessibility standards (WCAG 2.1) without sacrificing the <20ms latency performance targets.

Key improvements include:
- **Navigation:** Consolidated Dashboard with a Sidebar.
- **Visuals:** Fluent Design System with high-contrast typography and rounded corners.
- **UX:** Streamlined Host/Client workflows with "Recent Connections" and auto-discovery.
- **Telemetry:** Visual data dashboards (Gauges/Sparklines) replacing raw text lists.

---

## 2. UI/UX Audit Report

### 2.1 Window Layouts & Navigation
- **Issue:** Absolute Positioning (`ImGuiCond_FirstUseEver`).
- **Impact:** Misplaced or cut-off UI on varied monitor setups.
- **Solution:** Implement a responsive WinUI 3 layout that scales with DPI and window size.

### 2.2 Forms & Input Controls
- **Issue:** Missing Input Validation for IP addresses.
- **Impact:** Critical failure risk on incorrect input.
- **Solution:** Real-time regex validation and mask-based input fields.

### 2.3 Accessibility
- **Issue:** No Screen Reader support.
- **Impact:** Complete lack of usability for low-vision users.
- **Solution:** Native MSAA/UIA support via WinUI 3 framework.

*(Full details in AUDIT_REPORT.md)*

---

## 3. Redesign Proposal & Strategy
The core strategy is to decouple the **Presentation Layer** from the **Streaming Engine**.

1.  **Visual Language:** Adopt Microsoft's **Fluent Design**. Use the "Mica" material for the main window and "Acrylic" for the Telemetry overlay to ensure a "native" Windows feel.
2.  **Navigation Model:** Replace floating windows with a **Navigation View**. The app will feel like a single cohesive tool rather than a collection of debug overlays.
3.  **Modernization Pillars:** Consistency, Discoverability, and Performance.

---

## 4. Screen-by-Screen Analysis

### Screen 1: Dashboard (Home)
- **Purpose:** Central session control.
- **Current Problem:** No clear "Home" state; Host/Client modes are toggles in a small window.
- **Redesign:** Features large "Action Cards" for [Host] and [Connect], a system health banner, and a "Last Session" summary.

### Screen 2: Telemetry Overlay
- **Purpose:** In-stream monitoring.
- **Current Problem:** Wall of text that is hard to read during fast gameplay.
- **Redesign:** A "HUD" style overlay. Use color-coded sparklines (Latency) and gauges (Bitrate) for instant glanceability.

*(Full details in UX_STRATEGY.md)*

---

## 5. Design System & Component Library

### 5.1 Foundation
- **Colors:** Primary Accent (`#0078D4`), Surface (`#2B2B2B`), Background (`#202020`).
- **Typography:** Segoe UI Variable (Headers 28px, Body 14px, Monospace 14px for data).
- **Spacing:** 4px grid system with 16px card spacing.

### 5.2 Component Library
- **Cards:** Integrated containers with subtle shadows and 8px rounded corners.
- **Buttons:** Fluent-style Primary (Solid) and Secondary (Outline) buttons.
- **Input:** Validated text fields and standard toggle switches.
- **Telemetry Charts:** Custom sparkline and gauge controls for real-time visualization.

---

## 6. Technical Refactoring Plan

### 6.1 Architecture: Hybrid Bridge
- **Frontend (C#):** Handles UI, state, connection history, and settings management.
- **Core (C++):** Exposes a stable C-API via `ParsecCore.dll`.
- **Bridge:** Use **P/Invoke** or **C++/WinRT** to pass configuration and retrieve telemetry stats.

### 6.2 Implementation Roadmap
- **Phase A:** Core Refactoring (Extracting DLL API).
- **Phase B:** WinUI 3 Shell & Design System implementation.
- **Phase C:** Bridging & Session Lifecycle management.
- **Phase D:** Overlay porting and final accessibility tuning.

---

## 7. Priority Matrix

| Category | High Impact / Low Effort | High Impact / High Effort |
| :--- | :--- | :--- |
| **Critical** | Input Validation; Error Handling. | C# Frontend Migration. |
| **High** | Dark Mode; Telemetry Color-coding. | Connection History; Auto-discovery. |
| **Medium** | "Copy IP" Button; Tooltips. | Visual Sparklines & Gauges. |

---

## 8. Conclusion
By following this roadmap, Parsec-Lite will evolve from a high-performance prototype into a professional, enterprise-grade streaming application. The proposed redesign balances modern aesthetics with the project's strict performance requirements, ensuring that the user experience finally matches the quality of the underlying technology.
