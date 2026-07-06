import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme"
import "../components"

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.adaptiveMargin
        spacing: Theme.spacingLarge

        Text {
            text: "Settings"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeH1
            font.weight: Font.Bold
            color: Theme.textPrimary
        }

        // --- TAB NAVIGATION ---
        RowLayout {
            spacing: Theme.spacingMedium
            NexusButton {
                text: "General"; primary: settingsStack.currentIndex === 0
                onClicked: settingsStack.currentIndex = 0
            }
            NexusButton {
                text: "Streaming & Performance"; primary: settingsStack.currentIndex === 1
                onClicked: settingsStack.currentIndex = 1
            }
            NexusButton {
                text: "Network"; primary: settingsStack.currentIndex === 2
                onClicked: settingsStack.currentIndex = 2
            }
            NexusButton {
                text: "Appearance"; primary: settingsStack.currentIndex === 3
                onClicked: settingsStack.currentIndex = 3
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        StackLayout {
            id: settingsStack
            Layout.fillWidth: true
            Layout.fillHeight: true

            // --- GENERAL TAB ---
            ScrollView {
                clip: true
                ColumnLayout {
                    width: settingsStack.width; spacing: Theme.spacingLarge
                    NexusCard {
                        Layout.fillWidth: true; title: "User Identity"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: Theme.spacingLarge; spacing: Theme.spacingMedium
                            SettingRow {
                                label: "Display Name"
                                description: "How you appear to remote hosts and clients."
                                control: NexusInput {
                                    text: backend.system.username
                                    onTextChanged: backend.system.username = text
                                    Layout.preferredWidth: 300
                                }
                            }
                        }
                    }
                }
            }

            // --- STREAMING & PERFORMANCE TAB ---
            ScrollView {
                clip: true
                ColumnLayout {
                    width: settingsStack.width; spacing: Theme.spacingLarge
                    NexusCard {
                        Layout.fillWidth: true; title: "Encoding Strategy"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: Theme.spacingLarge; spacing: Theme.spacingLarge

                            SettingRow {
                                label: "Hardware Acceleration"
                                description: "Use dedicated GPU silicon for encoding/decoding. Significant latency reduction."
                                hasTooltip: true
                                tooltipText: "Highly recommended for gaming. Uses NVENC, QSV, or AMF depending on your GPU."
                                control: Switch {
                                    checked: backend.system.useHardwareEncoding
                                    onClicked: backend.system.useHardwareEncoding = checked
                                }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                            SettingRow {
                                label: "Encoder Preset"
                                description: "Balance between image fidelity and processing speed."
                                control: NexusSegmentedControl {
                                    model: ["Performance", "Balanced", "Quality"]
                                    currentIndex: backend.system.encoderPreset
                                    onActivated: (index) => backend.system.encoderPreset = index
                                    Layout.preferredWidth: 400
                                }
                            }
                        }
                    }
                }
            }

            // --- NETWORK TAB ---
            ScrollView {
                clip: true
                ColumnLayout {
                    width: settingsStack.width; spacing: Theme.spacingLarge
                    NexusCard {
                        Layout.fillWidth: true; title: "Connectivity"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: Theme.spacingLarge; spacing: Theme.spacingLarge
                            SettingRow {
                                label: "Primary Network Interface"
                                description: "The interface used for local discovery and streaming traffic."
                                control: NexusComboBox {
                                    model: backend.system.networkInterfaces
                                    Layout.preferredWidth: 400
                                }
                            }
                        }
                    }
                }
            }

            // --- APPEARANCE TAB ---
            ScrollView {
                clip: true
                ColumnLayout {
                    width: settingsStack.width; spacing: Theme.spacingLarge
                    NexusCard {
                        Layout.fillWidth: true; title: "Visual Experience"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: Theme.spacingLarge; spacing: Theme.spacingLarge
                            SettingRow {
                                label: "Dark Mode"
                                description: "Switch between light and dark UI themes."
                                control: Switch {
                                    checked: backend.theme.darkMode
                                    onClicked: backend.theme.darkMode = checked
                                }
                            }
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
        property bool hasTooltip: false
        property string tooltipText: ""
        property Item control: null

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            RowLayout {
                spacing: 8
                Text { text: label; color: Theme.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
                Rectangle {
                    visible: hasTooltip; width: 16; height: 16; radius: 8; color: Theme.surface
                    Text { anchors.centerIn: parent; text: "?"; color: Theme.textSecondary; font.pixelSize: 10; font.weight: Font.Bold }
                    ToolTip.visible: mouseArea.containsMouse
                    ToolTip.text: tooltipText
                    MouseArea { id: mouseArea; anchors.fill: parent; hoverEnabled: true }
                }
            }
            Text { text: description; color: Theme.textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }

        Item {
            data: [control]
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: control ? control.implicitWidth : 0
            implicitHeight: control ? control.implicitHeight : 0
        }
    }
}
