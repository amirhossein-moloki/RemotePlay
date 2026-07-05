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

        // --- GENERAL SETTINGS ---
        Text {
            text: "General"
            font.family: Theme.fontFamily
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.primary
            Layout.topMargin: Theme.spacingSmall
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
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingSmall
                    Text { text: "Display Name"; color: Theme.textPrimary; font.pixelSize: 13 }
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
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingSmall
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Dark Mode"; color: Theme.textPrimary; Layout.fillWidth: true; font.pixelSize: 13 }
                        Switch { checked: backend.theme.darkMode; onClicked: backend.theme.darkMode = checked }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Interface Scaling"; color: Theme.textPrimary; Layout.fillWidth: true; font.pixelSize: 13 }
                        ComboBox { model: ["Auto", "100%", "125%", "150%"]; Layout.preferredWidth: 100 }
                    }
                }
            }
        }

        // --- ADVANCED SETTINGS ---
        Text {
            text: "Advanced (Streaming & Performance)"
            font.family: Theme.fontFamily
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.primary
            Layout.topMargin: Theme.spacingLarge
        }

        GridLayout {
            columns: Theme.isSmall ? 1 : 2
            columnSpacing: Theme.adaptiveCardSpacing
            rowSpacing: Theme.adaptiveCardSpacing
            Layout.fillWidth: true

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                title: "Encoder & Video"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingSmall

                    SettingRow {
                        label: "Hardware Acceleration"
                        description: "Use GPU for faster encoding/decoding"
                        control: Switch {
                            checked: backend.system.useHardwareEncoding
                            onClicked: backend.system.useHardwareEncoding = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                    SettingRow {
                        label: "Encoder Preset"
                        description: "Quality vs Performance tradeoff"
                        control: ComboBox {
                            model: ["Performance", "Balanced", "Quality"]
                            currentIndex: backend.system.encoderPreset
                            onActivated: (index) => { backend.system.encoderPreset = index }
                        }
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                title: "Network & Bitrate"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingSmall

                    SettingRow {
                        label: "Target Bitrate"
                        description: "Higher means better quality but more bandwidth"
                        control: RowLayout {
                            NexusInput { text: "5000"; Layout.preferredWidth: 80 }
                            Text { text: "kbps"; color: Theme.textSecondary; font.pixelSize: 11 }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                    SettingRow {
                        label: "Target FPS"
                        description: "Frames per second for the stream"
                        control: ComboBox {
                            model: ["30", "60", "120", "144"]
                            currentIndex: 1
                        }
                    }
                }
            }
        }
    }

    // --- REUSABLE COMPONENTS ---
    component SettingRow : RowLayout {
        property string label: ""
        property string description: ""
        property Item control: null

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text { text: label; color: Theme.textPrimary; font.pixelSize: 13; font.weight: Font.Medium }
            Text { text: description; color: Theme.textSecondary; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }

        Item {
            data: [control]
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: control ? control.implicitWidth : 0
            implicitHeight: control ? control.implicitHeight : 0
        }
    }
}
