import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme"
import "../components"

ScrollView {
    anchors.fill: parent
    contentWidth: availableWidth

    ColumnLayout {
        width: parent.width
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingLarge

        NexusCard {
            Layout.fillWidth: true
            title: "Appearance"
            subtitle: "Personalize your workspace experience"

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingMedium

                RowLayout {
                    Layout.fillWidth: true
                    Column {
                        Layout.fillWidth: true
                        Text { text: "High Contrast Mode"; color: Theme.textPrimary; font.pixelSize: 14 }
                        Text { text: "Optimize visibility for accessibility"; color: Theme.textSecondary; font.pixelSize: 12 }
                    }
                    Switch {
                        checked: false
                        palette.active.highlight: Theme.primary
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border; opacity: 0.5 }

                RowLayout {
                    Layout.fillWidth: true
                    Column {
                        Layout.fillWidth: true
                        Text { text: "Accent Color"; color: Theme.textPrimary; font.pixelSize: 14 }
                        Text { text: "Choose your primary brand color"; color: Theme.textSecondary; font.pixelSize: 12 }
                    }
                    Row {
                        spacing: 8
                        Repeater {
                            model: [Theme.primary, "#EF4444", "#10B981", Theme.accent]
                            Rectangle {
                                width: 24; height: 24; radius: 12
                                color: modelData
                                border.width: 2
                                border.color: Theme.primary === modelData ? Theme.textPrimary : "transparent"
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor }
                            }
                        }
                    }
                }
            }
        }

        NexusCard {
            Layout.fillWidth: true
            title: "Streaming Defaults"
            subtitle: "Global settings for new sessions"

            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: Theme.spacingLarge
                rowSpacing: Theme.spacingMedium

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Default Target Bitrate (kbps)"; color: Theme.textSecondary; font.pixelSize: 12 }
                    NexusInput { Layout.fillWidth: true; text: "5000" }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Default Target FPS"; color: Theme.textSecondary; font.pixelSize: 12 }
                    NexusInput { Layout.fillWidth: true; text: "60" }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Audio Quality"; color: Theme.textSecondary; font.pixelSize: 12 }
                    ComboBox { Layout.fillWidth: true; model: ["High (192kbps)", "Medium (128kbps)", "Low (64kbps)"] }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Hardware Encoding"; color: Theme.textSecondary; font.pixelSize: 12 }
                    Switch { checked: true; palette.active.highlight: Theme.primary }
                }
            }
        }

        NexusCard {
            Layout.fillWidth: true
            title: "Application Environment"

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingMedium

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Developer Mode"; color: Theme.textPrimary; Layout.fillWidth: true }
                    Switch { checked: false }
                }

                NexusButton {
                    text: "Reset to Defaults"
                    primary: false
                    Layout.alignment: Qt.AlignRight
                }
            }
        }
    }
}
