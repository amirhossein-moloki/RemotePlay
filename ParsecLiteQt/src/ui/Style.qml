import QtQuick

pragma Singleton

QtObject {
    // Colors
    readonly property color backgroundDark: "#121212"
    readonly property color surfaceDark: "#1E1E1E"
    readonly property color backgroundLight: "#F5F5F7"
    readonly property color surfaceLight: "#FFFFFF"

    readonly property color accent: "#007AFF"
    readonly property color accentHover: "#0062CC"

    readonly property color textPrimaryDark: "#FFFFFF"
    readonly property color textSecondaryDark: "#A0A0A0"
    readonly property color textPrimaryLight: "#1D1D1F"
    readonly property color textSecondaryLight: "#86868B"

    readonly property color borderDark: "#333333"
    readonly property color borderLight: "#E5E5E5"

    // Current Theme Accessors (based on AppBridge)
    property color background: appBridge.darkMode ? backgroundDark : backgroundLight
    property color surface: appBridge.darkMode ? surfaceDark : surfaceLight
    property color textPrimary: appBridge.darkMode ? textPrimaryDark : textPrimaryLight
    property color textSecondary: appBridge.darkMode ? textSecondaryDark : textSecondaryLight
    property color border: appBridge.darkMode ? borderDark : borderLight

    // Spacing
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 16
    readonly property int spacingLarge: 24
    readonly property int radius: 8

    // Typography
    readonly property font headerFont: Qt.font({
        family: "Segoe UI Variable Display, Segoe UI, Roboto, Helvetica",
        pixelSize: 24,
        weight: Font.Bold
    })

    readonly property font bodyFont: Qt.font({
        family: "Segoe UI Variable Text, Segoe UI, Roboto, Helvetica",
        pixelSize: 14
    })

    readonly property font smallFont: Qt.font({
        family: "Segoe UI Variable Text, Segoe UI, Roboto, Helvetica",
        pixelSize: 12
    })
}
