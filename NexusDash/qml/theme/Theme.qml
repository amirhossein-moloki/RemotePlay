pragma Singleton
import QtQuick

QtObject {
    id: root
    readonly property bool isDark: backend.theme.darkMode

    // Window metrics for responsiveness
    property int windowWidth: 1200
    property int windowHeight: 800

    readonly property bool isSmall: windowWidth < 1000
    readonly property bool isMedium: windowWidth >= 1000 && windowWidth < 1400
    readonly property bool isLarge: windowWidth >= 1400

    // Adaptive Spacings (8pt Grid System)
    readonly property int adaptiveMargin: isSmall ? spacingMedium : (isMedium ? spacingLarge : spacingHuge)
    readonly property int adaptiveSpacing: isSmall ? spacingSmall : spacingMedium
    readonly property int adaptiveCardSpacing: isSmall ? spacingMedium : spacingLarge

    // Colors - Enterprise Navy/Charcoal Palette
    readonly property color background: "#0F172A"      // Deep Navy
    readonly property color panel: "#1E293B"           // Charcoal Navy
    readonly property color surface: "#334155"         // Lighter Slate/Charcoal
    readonly property color surfaceSecondary: "#475569"

    readonly property color primary: "#3B82F6"         // Modern Blue
    readonly property color accent: "#8B5CF6"          // Purple Accent
    readonly property color success: "#22C55E"         // Green
    readonly property color warning: "#F59E0B"         // Amber/Yellow
    readonly property color danger: "#EF4444"          // Red
    readonly property color active: "#3B82F6"          // Blue for active processes

    // Semantic status colors
    readonly property color statusGood: success
    readonly property color statusFair: warning
    readonly property color statusPoor: danger

    readonly property color textPrimary: "#F8FAFC"
    readonly property color textSecondary: "#94A3B8"
    readonly property color border: "#334155"

    // Functional Colors
    readonly property color accentHover: "#7C3AED"
    readonly property color primaryHover: "#2563EB"

    // Spacing (8pt Grid System)
    readonly property int spacingTiny: 4
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 16
    readonly property int spacingLarge: 24
    readonly property int spacingHuge: 32
    readonly property int spacingExtraHuge: 48
    readonly property int spacingGiga: 64

    // Radius
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12

    // Typography
    readonly property string fontFamily: "Inter, Segoe UI Variable, -apple-system, sans-serif"
    readonly property string monoFontFamily: "JetBrains Mono, Fira Code, Consolas, monospace"

    // Typography Sizes
    readonly property int fontSizeH1: 32
    readonly property int fontSizeH2: 24
    readonly property int fontSizeH3: 18
    readonly property int fontSizeBody: 14
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeTiny: 10
}
