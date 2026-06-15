pragma Singleton
import QtQuick

QtObject {
    id: root
    readonly property bool isDark: backend.theme.darkMode

    // Colors - Refined for Premium Dashboard
    readonly property color background: "#0B1020"
    readonly property color panel: "#111827"
    readonly property color surface: "#151D2E"
    readonly property color surfaceSecondary: "#1E293B"

    readonly property color primary: "#2563EB"
    readonly property color accent: "#8B5CF6"
    readonly property color success: "#10B981"
    readonly property color warning: "#F59E0B"
    readonly property color danger: "#EF4444"

    readonly property color textPrimary: "#F8FAFC"
    readonly property color textSecondary: "#94A3B8"
    readonly property color border: "#1E293B"

    // Functional Colors
    readonly property color accentHover: "#7C3AED"
    readonly property color primaryHover: "#1D4ED8"

    // Spacing (4px Grid System)
    readonly property int spacingTiny: 4
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 16
    readonly property int spacingLarge: 24
    readonly property int spacingHuge: 32

    // Radius
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12

    // Typography
    readonly property string fontFamily: "Segoe UI Variable, Inter, -apple-system, sans-serif"
    readonly property string monoFontFamily: "Consolas, 'Courier New', monospace"
}
