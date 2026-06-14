import QtQuick
import QtQuick.Layouts
import "../theme"
import "../components"

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingLarge

        Text {
            text: "Dashboard"
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
