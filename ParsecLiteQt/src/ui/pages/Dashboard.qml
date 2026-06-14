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
            text: "Welcome back, " + appBridge.userName
            font: Style.headerFont
            color: Style.textPrimary
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Style.spacingLarge

            Card {
                title: "Host a Session"
                Layout.fillWidth: true
                content: ColumnLayout {
                    anchors.fill: parent
                    spacing: Style.spacingMedium
                    Text {
                        text: "Allow others to connect to this computer for low-latency streaming."
                        font: Style.bodyFont
                        color: Style.textSecondary
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    CustomButton {
                        text: "Start Hosting"
                        Layout.fillWidth: true
                        onClicked: appBridge.startHosting()
                    }
                }
            }

            Card {
                title: "Connect to Host"
                Layout.fillWidth: true
                content: ColumnLayout {
                    anchors.fill: parent
                    spacing: Style.spacingMedium
                    TextField {
                        id: ipInput
                        placeholderText: "Enter IP Address (e.g. 192.168.1.15)"
                        Layout.fillWidth: true
                        font: Style.bodyFont
                        padding: 10
                        background: Rectangle {
                            radius: 6
                            color: Style.background
                            border.color: ipInput.activeFocus ? Style.accent : Style.border
                        }
                    }
                    CustomButton {
                        text: "Connect"
                        Layout.fillWidth: true
                        onClicked: appBridge.connectToHost(ipInput.text)
                    }
                }
            }
        }

        Text {
            text: "Recent Connections"
            font: Style.headerFont
            font.pixelSize: 20
            color: Style.textPrimary
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            interactive: false
            model: appBridge.recentSessions
            delegate: Rectangle {
                width: parent.width
                height: 60
                color: "transparent"

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Style.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Style.spacingSmall
                    anchors.rightMargin: Style.spacingSmall

                    ColumnLayout {
                        spacing: 2
                        Text {
                            text: modelData.name
                            font: Style.bodyFont
                            font.bold: true
                            color: Style.textPrimary
                        }
                        Text {
                            text: modelData.ip
                            font: Style.smallFont
                            color: Style.textSecondary
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: modelData.lastUsed
                        font: Style.smallFont
                        color: Style.textSecondary
                    }

                    CustomButton {
                        text: "Quick Connect"
                        backgroundColor: "transparent"
                        border.color: Style.border
                        onClicked: appBridge.connectToHost(modelData.ip)
                    }
                }
            }
        }
    }
}
