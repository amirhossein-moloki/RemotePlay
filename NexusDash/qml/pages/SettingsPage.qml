import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme"
import "../components"

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
            height: 200
            title: "Appearance"

            ColumnLayout {
                anchors.fill: parent
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
                        onToggled: backend.theme.darkMode = checked
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
            height: 150
            title: "Account"

            RowLayout {
                anchors.fill: parent
                spacing: Theme.spacingLarge

                NexusInput {
                    Layout.fillWidth: true
                    placeholderText: "Username"
                    text: "AdminUser"
                }

                NexusButton {
                    text: "Save Changes"
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
