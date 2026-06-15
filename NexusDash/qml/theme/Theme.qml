pragma Singleton
import QtQuick

QtObject {
    id: root
    readonly property bool isDark: backend.theme.darkMode

    // Colors
    readonly property color background: root.isDark ? "#0F0F12" : "#F5F7FA"
    readonly property color surface: root.isDark ? "#1A1A20" : "#FFFFFF"
    readonly property color surfaceSecondary: root.isDark ? "#25252E" : "#EDF2F7"

    readonly property color accent: "#3B82F6"
    readonly property color accentHover: "#2563EB"

    readonly property color textPrimary: root.isDark ? "#FFFFFF" : "#1A202C"
    readonly property color textSecondary: root.isDark ? "#A0AEC0" : "#4A5568"
    readonly property color border: root.isDark ? "#2D3748" : "#E2E8F0"

    // Spacing
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
    readonly property string fontFamily: "Segoe UI, Inter, sans-serif"
}
