pragma Singleton
import QtQuick

QtObject {
    id: root
    readonly property bool isDark: backend.theme.darkMode

    // Brand Colors
    readonly property color background: "#0B1020"
    readonly property color panel: "#111827"
    readonly property color card: "#151D2E"

    readonly property color primary: "#2563EB"
    readonly property color accent: "#8B5CF6"

    readonly property color success: "#10B981"
    readonly property color warning: "#F59E0B"
    readonly property color danger: "#EF4444"

    readonly property color textPrimary: "#F8FAFC"
    readonly property color textSecondary: "#94A3B8"

    // UI Functional Colors (mapped for compatibility)
    readonly property color surface: card
    readonly property color surfaceSecondary: panel
    readonly property color border: "#1E293B"
    readonly property color accentHover: Qt.lighter(primary, 1.2)

    // Spacing (4px grid system)
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
    readonly property string fontFamily: "Segoe UI Variable, Inter, SF Pro Display, sans-serif"

    // Shadows
    readonly property var shadowDefault: { "color": "#000000", "radius": 10, "samples": 15 }
}
