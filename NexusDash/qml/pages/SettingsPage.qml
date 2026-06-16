import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import NexusDash

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingLarge

        Text {
            text: "Settings"
            font.family: Theme.fontFamily
            font.pixelSize: 28
            font.weight: Font.Bold
            color: Theme.textPrimary
        }

        NexusCard {
            Layout.fillWidth: true
            implicitHeight: 150
            title: "User Profile"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingMedium

                Text {
                    text: "Display Name"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                }

                NexusInput {
                    id: usernameInput
                    Layout.fillWidth: true
                    text: backend.system.username
                    placeholderText: "Enter your username"
                    onTextChanged: backend.system.username = text
                }
            }
        }

        NexusCard {
            Layout.fillWidth: true
            implicitHeight: 200
            title: "Appearance"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingMedium

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Dark Mode"
                        font.family: Theme.fontFamily
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                    }
                    Switch {
                        checked: backend.theme.darkMode
                        onClicked: backend.theme.darkMode = checked
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Accent Color"
                        font.family: Theme.fontFamily
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                    }
                    Row {
                        spacing: 8
                        Repeater {
                            model: ["#3B82F6", "#EF4444", "#10B981", "#F59E0B"]
                            Rectangle {
                                width: 24; height: 24; radius: 12
                                color: modelData
                                border.width: 2
                                border.color: Theme.accent === modelData ? Theme.textPrimary : "transparent"
                                MouseArea { anchors.fill: parent; onClicked: {} /* Demo only */ }
                            }
                        }
                    }
                }
            }
        }

        NexusCard {
            Layout.fillWidth: true
            implicitHeight: 150
            title: "Network"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingMedium

                Text {
                    text: "Preferred Interface"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                }

                ComboBox {
                    Layout.fillWidth: true
                    model: backend.system.networkInterfaces
                }
            }
        }

        NexusCard {
            Layout.fillWidth: true
            implicitHeight: 150
            title: "Streaming Defaults"

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingLarge

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Default Bitrate (kbps)"; color: Theme.textSecondary; font.pixelSize: 12 }
                    NexusInput {
                        Layout.fillWidth: true
                        text: "5000"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Default FPS"; color: Theme.textSecondary; font.pixelSize: 12 }
                    NexusInput {
                        Layout.fillWidth: true
                        text: "60"
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
