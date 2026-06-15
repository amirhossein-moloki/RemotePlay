import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme"
import "../components"

ScrollView {
    id: scrollRoot
    anchors.fill: parent
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    ColumnLayout {
        width: parent.width
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingLarge

        // Row 1: Session Control
        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: Theme.spacingLarge
            rowSpacing: Theme.spacingLarge

            // Start Hosting Card
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                title: "Start Hosting"
                subtitle: "Share your system across the network"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingMedium

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingTiny
                        Text { text: "Network Adapter"; color: Theme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: interfaceCombo
                            Layout.fillWidth: true
                            model: backend.system.networkInterfaces
                            textRole: "ip"

                            delegate: ItemDelegate {
                                width: parent.width
                                contentItem: Column {
                                    Text { text: modelData.name; font.bold: true; color: Theme.textPrimary }
                                    Text { text: modelData.ip + " (" + modelData.type + ")"; font.pixelSize: 11; color: Theme.textSecondary }
                                }
                                background: Rectangle { color: highlighted ? Qt.rgba(1,1,1,0.1) : "transparent" }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: "Bitrate (Mbps)"; color: Theme.textSecondary; font.pixelSize: 12 }
                            NexusInput { Layout.fillWidth: true; text: "10"; id: hostBitrate }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: "FPS"; color: Theme.textSecondary; font.pixelSize: 12 }
                            NexusInput { Layout.fillWidth: true; text: "60"; id: hostFps }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium
                        NexusButton {
                            text: "Start Hosting"
                            primary: true
                            Layout.fillWidth: true
                            enabled: !backend.system.isSessionActive
                            onClicked: backend.system.startHost(interfaceCombo.currentText, parseInt(hostBitrate.text) * 1000, parseInt(hostFps.text))
                        }
                        NexusButton {
                            text: "Stop"
                            primary: false
                            Layout.preferredWidth: 80
                            enabled: backend.system.isSessionActive && backend.system.connectionState === "Hosting"
                            onClicked: backend.system.stopSession()
                        }
                    }
                }
            }

            // Connect to Host Card
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                title: "Connect to Host"
                subtitle: "Join a remote streaming session"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingMedium

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingTiny
                        Text { text: "Host IP Address"; color: Theme.textSecondary; font.pixelSize: 12 }
                        NexusInput {
                            id: hostIpInput
                            placeholderText: "e.g. 192.168.1.100"
                            Layout.fillWidth: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingTiny
                        Text { text: "Recent Connections"; color: Theme.textSecondary; font.pixelSize: 12 }
                        ListView {
                            Layout.fillWidth: true
                            height: 60
                            clip: true
                            model: ["192.168.1.50", "10.0.0.12"]
                            delegate: Item {
                                width: parent.width; height: 30
                                Text { text: modelData; color: Theme.accent; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                                MouseArea { anchors.fill: parent; onClicked: hostIpInput.text = modelData }
                            }
                        }
                    }

                    NexusButton {
                        text: "Connect to Host"
                        primary: true
                        Layout.fillWidth: true
                        enabled: !backend.system.isSessionActive
                        onClicked: backend.system.startClient(hostIpInput.text, 5000, 60)
                    }
                }
            }

            // Performance Info Card
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                title: "Performance Info"
                subtitle: "Real-time streaming metrics"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingMedium

                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        rowSpacing: Theme.spacingSmall

                        Repeater {
                            model: [
                                { l: "Encoder", v: "NVENC H.264" },
                                { l: "Decoder", v: "D3D11 HW" },
                                { l: "Quality", v: "High" },
                                { l: "FEC", v: "Enabled (XOR)" }
                            ]
                            Row {
                                spacing: Theme.spacingSmall
                                Text { text: modelData.l + ":"; color: Theme.textSecondary; font.pixelSize: 12; width: 60 }
                                Text { text: modelData.v; color: Theme.textPrimary; font.pixelSize: 12; font.weight: Font.Medium }
                            }
                        }
                    }

                    // Real-time mini graph placeholder
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Qt.rgba(1,1,1,0.03)
                        radius: Theme.radiusSmall
                        border.color: Theme.border

                        Text {
                            anchors.centerIn: parent
                            text: "Realtime Graph Placeholder"
                            color: Theme.textSecondary
                            font.pixelSize: 11
                        }

                        // Simple animated line to simulate activity
                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.clearRect(0, 0, width, height);
                                ctx.strokeStyle = Theme.accent;
                                ctx.lineWidth = 2;
                                ctx.beginPath();
                                ctx.moveTo(0, height/2);
                                for(var i=0; i<width; i+=10) {
                                    ctx.lineTo(i, height/2 + (Math.random()-0.5)*20);
                                }
                                ctx.stroke();
                            }
                            Timer { interval: 500; running: true; repeat: true; onTriggered: parent.requestPaint() }
                        }
                    }
                }
            }
        // Row 2: System Monitoring
        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: Theme.spacingLarge
            rowSpacing: Theme.spacingLarge

            // CPU Usage Card
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                title: "CPU Usage"

                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingLarge

                    // Circular Indicator
                    Item {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 100

                        Canvas {
                            anchors.fill: parent
                            property real usage: backend.system.cpuUsage / 100.0
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.clearRect(0, 0, width, height);
                                var centerX = width / 2;
                                var centerY = height / 2;
                                var radius = Math.min(width, height) / 2 - 5;

                                // Background circle
                                ctx.beginPath();
                                ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
                                ctx.strokeStyle = Theme.panel;
                                ctx.lineWidth = 8;
                                ctx.stroke();

                                // Progress arc
                                ctx.beginPath();
                                ctx.arc(centerX, centerY, radius, -Math.PI / 2, (-Math.PI / 2) + (2 * Math.PI * usage));
                                ctx.strokeStyle = Theme.primary;
                                ctx.lineWidth = 8;
                                ctx.lineCap = "round";
                                ctx.stroke();
                            }
                            onUsageChanged: requestPaint()
                        }

                        Text {
                            anchors.centerIn: parent
                            text: backend.system.cpuUsage.toFixed(0) + "%"
                            font.family: Theme.fontFamily
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            color: Theme.textPrimary
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSmall
                        Text { text: "System Load"; color: Theme.textSecondary; font.pixelSize: 12 }
                        Text { text: "Optimal Performance"; color: Theme.success; font.pixelSize: 14; font.weight: Font.Medium }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Qt.rgba(1,1,1,0.03)
                            radius: 4
                            // Sparkline placeholder
                        }
                    }
                }
            }

            // Memory Usage Card
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                title: "Memory Usage"

                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingLarge

                    // Circular Indicator
                    Item {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 100

                        Canvas {
                            anchors.fill: parent
                            property real usage: backend.system.memoryUsage / 100.0
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.clearRect(0, 0, width, height);
                                var centerX = width / 2;
                                var centerY = height / 2;
                                var radius = Math.min(width, height) / 2 - 5;

                                ctx.beginPath();
                                ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
                                ctx.strokeStyle = Theme.panel;
                                ctx.lineWidth = 8;
                                ctx.stroke();

                                ctx.beginPath();
                                ctx.arc(centerX, centerY, radius, -Math.PI / 2, (-Math.PI / 2) + (2 * Math.PI * usage));
                                ctx.strokeStyle = Theme.accent;
                                ctx.lineWidth = 8;
                                ctx.lineCap = "round";
                                ctx.stroke();
                            }
                            onUsageChanged: requestPaint()
                        }

                        Text {
                            anchors.centerIn: parent
                            text: backend.system.memoryUsage.toFixed(0) + "%"
                            font.family: Theme.fontFamily
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            color: Theme.textPrimary
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSmall
                        Text { text: "Physical RAM"; color: Theme.textSecondary; font.pixelSize: 12 }
                        Text { text: "Stable Environment"; color: Theme.success; font.pixelSize: 14; font.weight: Font.Medium }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            color: Qt.rgba(1,1,1,0.03)
                            radius: 4
                        }
                    }
                }
            }

            // System Uptime Card
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                title: "System Uptime"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingMedium
                    alignment: Qt.AlignVCenter

                    Text {
                        text: backend.system.uptime
                        font.family: Theme.fontFamily
                        font.pixelSize: 36
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Row {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Theme.spacingSmall
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: Theme.success
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "System Health: Excellent"
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            color: Theme.textSecondary
                        }
                    }
                }
            }
        // Row 3: System Logs
        NexusCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "System Logs"
            subtitle: "Live event feed from the streaming core"

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingMedium

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMedium

                    NexusInput {
                        placeholderText: "Search logs..."
                        Layout.fillWidth: true
                        id: logSearch
                    }

                    ComboBox {
                        model: ["All Levels", "INFO", "WARN", "ERROR"]
                        Layout.preferredWidth: 150
                    }

                    NexusButton {
                        text: "Clear"
                        primary: false
                        Layout.preferredWidth: 80
                    }

                    NexusButton {
                        text: "Export"
                        primary: false
                        Layout.preferredWidth: 80
                    }
                }

                NexusTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: [
                        { name: "Level", role: "level", width: 80 },
                        { name: "Module", role: "module", width: 120 },
                        { name: "Message", role: "message", width: 600 },
                        { name: "Timestamp", role: "timestamp", width: 180 }
                    ]
                    model: backend.system.logs
                }
            }
        }
    }
}
