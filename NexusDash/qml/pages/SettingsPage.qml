import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import NexusDash

ScrollView {
    id: root
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.adaptiveCardSpacing
        anchors.margins: Theme.spacingTiny

        Text {
            text: "Settings"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.isSmall ? 24 : 28
            font.weight: Font.Bold
            color: Theme.textPrimary
            Layout.topMargin: Theme.spacingMedium
        }

        GridLayout {
            columns: Theme.isSmall ? 1 : 2
            columnSpacing: Theme.adaptiveCardSpacing
            rowSpacing: Theme.adaptiveCardSpacing
            Layout.fillWidth: true

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                title: "User Profile"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingSmall

                    Text {
                        text: "Display Name"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 13
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
                Layout.preferredHeight: 160
                title: "Appearance"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingSmall

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Dark Mode"
                            font.family: Theme.fontFamily
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            font.pixelSize: 13
                        }
                        Switch {
                            checked: backend.theme.darkMode
                            onClicked: backend.theme.darkMode = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: "Encoder Preset"
                                font.family: Theme.fontFamily
                                color: Theme.textPrimary
                                font.pixelSize: 13
                            }
                            Text {
                                text: "Optimization target for video encoding"
                                font.family: Theme.fontFamily
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                        }
                        ComboBox {
                            model: ["Performance", "Balanced", "Quality"]
                            currentIndex: backend.system.encoderPreset
                            onActivated: (index) => { backend.system.encoderPreset = index }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: "Auto-Approve Connections"
                                font.family: Theme.fontFamily
                                color: Theme.textPrimary
                                font.pixelSize: 13
                            }
                            Text {
                                text: "Automatically accept incoming stream requests"
                                font.family: Theme.fontFamily
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                        }
                        Switch {
                            checked: backend.system.autoApprove
                            onClicked: backend.system.autoApprove = checked
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
                            font.pixelSize: 13
                        }
                        Row {
                            spacing: 8
                            Repeater {
                                model: ["#3B82F6", "#EF4444", "#10B981", "#F59E0B"]
                                Rectangle {
                                    width: 20; height: 20; radius: 10
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
                Layout.preferredHeight: 160
                title: "Network"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingSmall

                    Text {
                        text: "Preferred Interface"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 13
                    }

                    ComboBox {
                        Layout.fillWidth: true
                        model: backend.system.networkInterfaces
                        font.family: Theme.fontFamily
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                title: "Streaming Defaults"

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    columns: Theme.isSmall ? 1 : 2
                    columnSpacing: Theme.spacingLarge

                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: "Default Bitrate (kbps)"; color: Theme.textSecondary; font.pixelSize: 11 }
                        NexusInput {
                            id: bitrateInput
                            Layout.fillWidth: true
                            text: "5000"
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: "Default FPS"; color: Theme.textSecondary; font.pixelSize: 11 }
                        NexusInput {
                            id: fpsInput
                            Layout.fillWidth: true
                            text: "60"
                        }
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                title: "Performance & Quality"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingSmall

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: "Hardware Acceleration"
                                font.family: Theme.fontFamily
                                color: Theme.textPrimary
                                font.pixelSize: 13
                            }
                            Text {
                                text: "Use GPU for faster encoding/decoding"
                                font.family: Theme.fontFamily
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                        }
                        Switch {
                            checked: backend.system.useHardwareEncoding
                            onClicked: backend.system.useHardwareEncoding = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: "Resolution Scale"
                                font.family: Theme.fontFamily
                                color: Theme.textPrimary
                                font.pixelSize: 13
                            }
                            Text {
                                text: "Reduce resolution for better performance"
                                font.family: Theme.fontFamily
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                        }
                        ComboBox {
                            model: ListModel {
                                ListElement { text: "100% (Native)"; value: 1.0 }
                                ListElement { text: "75%"; value: 0.75 }
                                ListElement { text: "50%"; value: 0.5 }
                            }
                            textRole: "text"
                            currentIndex: backend.system.resolutionScale === 1.0 ? 0 : (backend.system.resolutionScale === 0.75 ? 1 : 2)
                            onActivated: (index) => { backend.system.resolutionScale = model.get(index).value }
                        }
                    }
                }
            }
        }
    }
}
