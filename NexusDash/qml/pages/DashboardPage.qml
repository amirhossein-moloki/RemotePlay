import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme"
import "../components"

Item {
    id: root

    readonly property bool isActive: backend.system.isSessionActive

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingLarge

        Text {
            text: root.isActive ? "Live Session" : "Dashboard"
            font.family: Theme.fontFamily
            font.pixelSize: 28
            font.weight: Font.Bold
            color: Theme.textPrimary
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: Theme.spacingMedium
            rowSpacing: Theme.spacingMedium
            visible: !root.isActive

            NexusCard {
                Layout.fillWidth: true
                height: 160
                title: "Start Hosting"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingSmall

                    Text {
                        text: "Share your screen on the network"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    ComboBox {
                        id: interfaceCombo
                        Layout.fillWidth: true
                        model: backend.system.networkInterfaces
                        flat: true
                    }

                    NexusButton {
                        text: "Launch Host"
                        Layout.fillWidth: true
                        onClicked: backend.system.startHost(interfaceCombo.currentText, 5000, 60)
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                height: 160
                title: "Connect to Host"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingSmall

                    NexusInput {
                        id: hostIpInput
                        placeholderText: "Host IP (e.g. 192.168.1.5)"
                        Layout.fillWidth: true
                    }

                    NexusButton {
                        text: "Join Session"
                        Layout.fillWidth: true
                        onClicked: backend.system.startClient(hostIpInput.text, 5000, 60)
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                height: 160
                title: "Performance Info"
                Column {
                    anchors.centerIn: parent
                    Text { text: "Target: 60 FPS"; color: Theme.textPrimary; font.bold: true }
                    Text { text: "Bitrate: 5.0 Mbps"; color: Theme.textSecondary }
                    Text { text: "Low Latency: Enabled"; color: "#10B981" }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: Theme.spacingMedium
            rowSpacing: Theme.spacingMedium
            visible: root.isActive

            NexusCard {
                Layout.fillWidth: true
                height: 140
                title: "Latency"
                Text {
                    anchors.centerIn: parent
                    text: backend.system.e2eLatency.toFixed(1) + " ms"
                    font.pixelSize: 32; font.bold: true; color: Theme.accent
                }
            }

            NexusCard {
                Layout.fillWidth: true
                height: 140
                title: "Bitrate"
                Text {
                    anchors.centerIn: parent
                    text: backend.system.bitrate.toFixed(1) + " Mbps"
                    font.pixelSize: 32; font.bold: true; color: "#10B981"
                }
            }

            NexusCard {
                Layout.fillWidth: true
                height: 140
                title: "Control"
                NexusButton {
                    anchors.centerIn: parent
                    text: "Stop Session"
                    onClicked: backend.system.stopSession()
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: Theme.spacingMedium
            rowSpacing: Theme.spacingMedium

            NexusCard {
                Layout.fillWidth: true
                height: 140
                title: "CPU Usage"

                Column {
                    anchors.centerIn: parent
                    spacing: Theme.spacingSmall
                    Text {
                        text: backend.system.cpuUsage.toFixed(1) + "%"
                        font.family: Theme.fontFamily
                        font.pixelSize: 32
                        font.weight: Font.Bold
                        color: Theme.accent
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Rectangle {
                        width: 150; height: 8; radius: 4; color: Theme.surfaceSecondary
                        Rectangle {
                            width: parent.width * (backend.system.cpuUsage / 100.0)
                            height: parent.height; radius: 4; color: Theme.accent
                            Behavior on width { NumberAnimation { duration: 500 } }
                        }
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                height: 140
                title: "Memory"

                Column {
                    anchors.centerIn: parent
                    spacing: Theme.spacingSmall
                    Text {
                        text: backend.system.memoryUsage.toFixed(1) + "%"
                        font.family: Theme.fontFamily
                        font.pixelSize: 32
                        font.weight: Font.Bold
                        color: "#10B981"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Rectangle {
                        width: 150; height: 8; radius: 4; color: Theme.surfaceSecondary
                        Rectangle {
                            width: parent.width * (backend.system.memoryUsage / 100.0)
                            height: parent.height; radius: 4; color: "#10B981"
                            Behavior on width { NumberAnimation { duration: 500 } }
                        }
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                height: 140
                title: "Uptime"

                Text {
                    anchors.centerIn: parent
                    text: backend.system.uptime
                    font.family: Theme.fontFamily
                    font.pixelSize: 24
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }
            }
        }

        NexusCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "System Logs"

            NexusTable {
                anchors.fill: parent
                columns: [
                    { name: "Event", role: "t", width: 200 },
                    { name: "Description", role: "d", width: 400 },
                    { name: "Time", role: "s", width: 150 }
                ]
                model: [
                    { t: "System Update", d: "Security patches applied successfully.", s: "Just now" },
                    { t: "New Device", d: "MacBook Pro connected to the network.", s: "2 mins ago" },
                    { t: "Backup", d: "Cloud backup completed.", s: "1 hour ago" },
                    { t: "Login", d: "Admin logged in from 192.168.1.5", s: "3 hours ago" }
                ]
            }
        }
    }
}
