import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import NexusDash

ScrollView {
    id: root
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    clip: true

    readonly property bool isActive: backend.system.isSessionActive

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacingLarge
        anchors.margins: Theme.spacingLarge

        DashboardHeader {
            title: root.isActive ? "Live Session" : "Dashboard"
        }

        // FIRST ROW
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingLarge
            visible: !root.isActive

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 350
                title: "Start Hosting"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    RowLayout {
                        spacing: Theme.spacingMedium
                        Rectangle {
                            width: 48; height: 48; radius: 12
                            color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.1)
                            Text { anchors.centerIn: parent; text: "H"; color: Theme.primary; font.bold: true; font.pixelSize: 20 }
                        }
                        Column {
                            Text { text: "Share your system"; color: Theme.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
                            Text { text: "Across the local network"; color: Theme.textSecondary; font.pixelSize: 12 }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSmall
                        Text { text: "Select Network Interface"; color: Theme.textSecondary; font.pixelSize: 11; font.weight: Font.Bold }
                        NexusComboBox {
                            id: interfaceCombo
                            Layout.fillWidth: true
                            model: backend.system.networkInterfaces
                        }
                    }
                    Item { Layout.fillHeight: true }
                    NexusButton {
                        text: "Start Hosting"
                        Layout.fillWidth: true
                        onClicked: backend.system.startHost(interfaceCombo.currentText, 5000, 60)
                    }
                    NexusButton {
                        text: "Stop Hosting"
                        primary: false
                        Layout.fillWidth: true
                        onClicked: backend.system.stopSession()
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 350
                title: "Connect to Host"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    RowLayout {
                        spacing: Theme.spacingMedium
                        Rectangle {
                            width: 48; height: 48; radius: 12
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1)
                            Text { anchors.centerIn: parent; text: "C"; color: Theme.accent; font.bold: true; font.pixelSize: 20 }
                        }
                        Column {
                            Text { text: "Join a session"; color: Theme.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
                            Text { text: "Select interface and enter host IP"; color: Theme.textSecondary; font.pixelSize: 12 }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSmall
                        Text { text: "Local Interface"; color: Theme.textSecondary; font.pixelSize: 11; font.weight: Font.Bold }
                        NexusComboBox {
                            id: clientInterfaceCombo
                            Layout.fillWidth: true
                            model: backend.system.networkInterfaces
                        }
                    }
                    NexusInput {
                        id: hostIpInput
                        placeholderText: "Remote Host IP (e.g. 192.168.1.XX)"
                        Layout.fillWidth: true
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingTiny
                        Text { text: "Recent Hosts"; color: Theme.textSecondary; font.pixelSize: 11; font.weight: Font.Bold }
                        ListView {
                            Layout.fillWidth: true
                            height: 80
                            clip: true
                            model: ["192.168.1.10", "192.168.1.5"] // Real persistence would be a future enhancement
                            delegate: Item {
                                width: parent.width; height: 32
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: 2; radius: 4; color: Theme.surfaceSecondary
                                    Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Theme.textPrimary; font.pixelSize: 12 }
                                    MouseArea { anchors.fill: parent; onClicked: hostIpInput.text = modelData; cursorShape: Qt.PointingHandCursor }
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    NexusButton {
                        text: "Connect"
                        Layout.fillWidth: true
                        onClicked: backend.system.startClient(clientInterfaceCombo.currentText, hostIpInput.text, 5000, 60)
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 350
                title: "Performance Info"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    MetricRow { label: "Target FPS"; value: "60 FPS" }
                    MetricRow { label: "Target Bitrate"; value: "50 Mbps" }
                    MetricRow { label: "Quality Mode"; value: "High (H.264)" }
                    MetricRow { label: "RTT (Latency)"; value: backend.system.rtt.toFixed(1) + " ms" }
                    Item { Layout.fillHeight: true }
                    Text {
                        text: "Streaming engine is active. All metrics are live."
                        visible: root.isActive
                        color: Theme.success
                        font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true
                    }
                    Text {
                        text: "Metrics will be available during an active session."
                        visible: !root.isActive
                        color: Theme.textSecondary
                        font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                }
            }
        }

        // ACTIVE SESSION VIEW
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingLarge
            visible: root.isActive

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 250
                title: "Live Stream Metrics"
                RowLayout {
                    anchors.fill: parent
                    GridLayout {
                        columns: 2
                        columnSpacing: Theme.spacingHuge
                        MetricLarge { label: "FPS"; value: backend.system.fps.toFixed(0); color: Theme.success }
                        MetricLarge { label: "BITRATE"; value: backend.system.bitrate.toFixed(1); suffix: "Mb"; color: Theme.primary }
                        MetricLarge { label: "LATENCY"; value: backend.system.e2eLatency.toFixed(1); suffix: "ms"; color: Theme.accent }
                        MetricLarge { label: "LOSS"; value: (backend.system.lossRate * 100).toFixed(2) + "%"; color: Theme.warning }
                    }
                    Item { Layout.fillWidth: true }
                    NexusGraph {
                        Layout.preferredWidth: 300; Layout.preferredHeight: 120
                        dataModel: backend.system.fpsHistory
                        maxValue: 70; lineWeightColor: Theme.success
                    }
                }
            }
            NexusCard {
                Layout.preferredWidth: 200
                Layout.preferredHeight: 200
                title: "Control"
                NexusButton {
                    anchors.centerIn: parent
                    text: "Stop Session"
                    onClicked: backend.system.stopSession()
                }
            }
        }

        // SECOND ROW
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingLarge
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                title: "CPU Usage"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingMedium
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Theme.spacingLarge
                        Item {
                            width: 100; height: 100
                            Canvas {
                                id: cpuCanvas
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.beginPath(); ctx.arc(width/2, height/2, 45, 0, 2*Math.PI);
                                    ctx.strokeStyle = Theme.surfaceSecondary; ctx.lineWidth = 10; ctx.stroke();
                                    ctx.beginPath(); ctx.arc(width/2, height/2, 45, -Math.PI/2, (-Math.PI/2) + (backend.system.cpuUsage/100 * 2*Math.PI));
                                    ctx.strokeStyle = Theme.accent; ctx.lineWidth = 10; ctx.lineCap = "round"; ctx.stroke();
                                }
                                Connections {
                                    target: backend.system
                                    function onCpuUsageChanged() { cpuCanvas.requestPaint() }
                                }
                            }
                            Text { anchors.centerIn: parent; text: backend.system.cpuUsage.toFixed(0) + "%"; font.family: Theme.fontFamily; font.pixelSize: 20; font.weight: Font.Bold; color: Theme.textPrimary }
                        }
                        Column {
                            spacing: 4
                            Text { text: "Average Load"; color: Theme.textSecondary; font.pixelSize: 12 }
                            Text { text: "System Processor"; color: Theme.textPrimary; font.weight: Font.Medium; font.pixelSize: 14 }
                            Text { text: "Status: Optimal"; color: Theme.success; font.pixelSize: 12; font.weight: Font.Bold }
                        }
                    }
                    NexusGraph {
                        Layout.fillWidth: true; Layout.preferredHeight: 60
                        dataModel: backend.system.cpuHistory
                        maxValue: 100; lineWeightColor: Theme.accent
                    }
                }
            }
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                title: "Memory Usage"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingMedium
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Theme.spacingLarge
                        Item {
                            width: 100; height: 100
                            Canvas {
                                id: memoryCanvas
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.beginPath(); ctx.arc(width/2, height/2, 45, 0, 2*Math.PI);
                                    ctx.strokeStyle = Theme.surfaceSecondary; ctx.lineWidth = 10; ctx.stroke();
                                    ctx.beginPath(); ctx.arc(width/2, height/2, 45, -Math.PI/2, (-Math.PI/2) + (backend.system.memoryUsage/100 * 2*Math.PI));
                                    ctx.strokeStyle = Theme.success; ctx.lineWidth = 10; ctx.lineCap = "round"; ctx.stroke();
                                }
                                Connections {
                                    target: backend.system
                                    function onMemoryUsageChanged() { memoryCanvas.requestPaint() }
                                }
                            }
                            Text { anchors.centerIn: parent; text: backend.system.memoryUsage.toFixed(0) + "%"; font.family: Theme.fontFamily; font.pixelSize: 20; font.weight: Font.Bold; color: Theme.textPrimary }
                        }
                        Column {
                            spacing: 4
                            Text { text: "Used Memory"; color: Theme.textSecondary; font.pixelSize: 12 }
                            Text { text: "System Memory"; color: Theme.textPrimary; font.weight: Font.Medium; font.pixelSize: 14 }
                            Text { text: "Status: Balanced"; color: Theme.success; font.pixelSize: 12 }
                        }
                    }
                    NexusGraph {
                        Layout.fillWidth: true; Layout.preferredHeight: 60
                        dataModel: backend.system.memoryHistory
                        maxValue: 100; lineWeightColor: Theme.success
                    }
                }
            }
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                title: "System Uptime"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingMedium
                    Item { Layout.fillHeight: true }
                    Text { Layout.alignment: Qt.AlignHCenter; text: backend.system.uptime; font.family: Theme.monoFontFamily; font.pixelSize: 36; font.weight: Font.Bold; color: Theme.primary }
                    Text { Layout.alignment: Qt.AlignHCenter; text: "UPTIME SINCE LAST REBOOT"; font.pixelSize: 10; font.weight: Font.Black; color: Theme.textSecondary; font.letterSpacing: 1 }
                    Item { Layout.fillHeight: true }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSmall; Rectangle { width: 8; height: 8; radius: 4; color: Theme.success }
                        Text { text: "System Health: EXCELLENT"; color: Theme.textPrimary; font.pixelSize: 12; font.weight: Font.Bold }
                    }
                    NexusGraph {
                        Layout.fillWidth: true; Layout.preferredHeight: 40
                        dataModel: backend.system.latencyHistory
                        maxValue: 100; lineWeightColor: Theme.success; fillEnabled: false
                    }
                }
            }
        }

        // THIRD ROW: System Logs
        NexusCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "System Logs"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingMedium
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMedium
                    NexusInput { placeholderText: "Search logs..."; Layout.fillWidth: true; Layout.maximumWidth: 300 }
                    ComboBox { model: ["All Levels", "Info", "Warning", "Error"]; Layout.preferredWidth: 150 }
                    Item { Layout.fillWidth: true }
                    NexusButton { text: "Clear Logs"; primary: false; Layout.preferredWidth: 100 }
                    NexusButton { text: "Export"; primary: false; Layout.preferredWidth: 100 }
                }
                NexusTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: [
                        { name: "Level", role: "level", width: 80 },
                        { name: "Event", role: "event", width: 150 },
                        { name: "Description", role: "desc", width: 400 },
                        { name: "Timestamp", role: "time", width: 150 }
                    ]
                    model: backend.system.logModel
                }
            }
        }
    }

    // Components
    component MetricRow : RowLayout {
        property string label: ""
        property string value: ""
        Text { text: label; color: Theme.textSecondary; Layout.fillWidth: true; font.pixelSize: 13 }
        Text { text: value; color: Theme.textPrimary; font.weight: Font.Medium; font.pixelSize: 13 }
    }

    component MetricLarge : Column {
        id: metricRoot
        property string label: ""
        property string value: ""
        property string suffix: ""
        property color color: Theme.textPrimary
        Text { text: label; color: Theme.textSecondary; font.pixelSize: 10; font.weight: Font.Bold }
        Row {
            spacing: 2
            Text { text: value; color: metricRoot.color; font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.monoFontFamily }
            Text { text: suffix; color: Theme.textSecondary; font.pixelSize: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 4 }
        }
    }
}
