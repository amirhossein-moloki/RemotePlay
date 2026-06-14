import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ParsecLite.UI

// Relative import for components
import "../components"

ScrollView {
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: parent.width
        anchors.margins: Style.spacingLarge
        spacing: Style.spacingLarge

        Text {
            text: "Settings"
            font: Style.headerFont
            color: Style.textPrimary
        }

        Card {
            title: "General"
            content: ColumnLayout {
                anchors.fill: parent
                spacing: Style.spacingLarge

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Display Name"
                        font: Style.bodyFont
                        color: Style.textPrimary
                        Layout.fillWidth: true
                    }
                    TextField {
                        text: appBridge.userName
                        onEditingFinished: appBridge.userName = text
                        padding: 8
                        background: Rectangle {
                            implicitWidth: 200
                            radius: 4
                            color: Style.background
                            border.color: Style.border
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Dark Mode"
                        font: Style.bodyFont
                        color: Style.textPrimary
                        Layout.fillWidth: true
                    }
                    Switch {
                        checked: appBridge.darkMode
                        onToggled: appBridge.darkMode = checked
                    }
                }
            }
        }

        Card {
            title: "Streaming Performance"
            content: ColumnLayout {
                anchors.fill: parent
                spacing: Style.spacingMedium

                Text {
                    text: "Max Bitrate (Mbps)"
                    font: Style.bodyFont
                    color: Style.textPrimary
                }
                Slider {
                    Layout.fillWidth: true
                    from: 5
                    to: 50
                    value: 20
                }

                Text {
                    text: "Resolution Scaling"
                    font: Style.bodyFont
                    color: Style.textPrimary
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["100% (Native)", "75%", "50% (Performance)"]
                }
            }
        }
    }
}
