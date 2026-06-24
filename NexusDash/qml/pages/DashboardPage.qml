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
        spacing: Theme.adaptiveCardSpacing
        anchors.margins: Theme.spacingTiny // ScrollView already provides some space, we just need minimal here

        DashboardHeader {
            title: root.isActive ? "Live Session" : "Dashboard"
        }

        // FIRST ROW - ADAPTIVE GRID
        GridLayout {
            Layout.fillWidth: true
            columnSpacing: Theme.adaptiveCardSpacing
            rowSpacing: Theme.adaptiveCardSpacing
            columns: Theme.isSmall ? 1 : 3
            visible: !root.isActive

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.isSmall ? 400 : 450
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
                Layout.preferredHeight: Theme.isSmall ? 400 : 450
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
                            model: ["127.0.0.1", "192.168.1.10", "192.168.1.5"]
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
                        onClicked: {
                            var ipRegex = /^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$/
                            if (!ipRegex.test(hostIpInput.text)) {
                                hostIpInput.isInvalid = true
                                hostIpInput.errorMessage = "لطفاً یک آدرس IP معتبر وارد کنید (مثلاً ۱۹۲.۱۶۸.۱.۱۰)"
                                return
                            }
                            hostIpInput.isInvalid = false
                            backend.system.startClient(clientInterfaceCombo.currentText, hostIpInput.text, 5000, 60)
                        }
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.isSmall ? 400 : 450
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
        GridLayout {
            Layout.fillWidth: true
            columnSpacing: Theme.adaptiveCardSpacing
            rowSpacing: Theme.adaptiveCardSpacing
            columns: Theme.isSmall ? 1 : 2
            visible: root.isActive

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.isSmall ? 350 : 250
                title: "Live Stream Metrics"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    GridLayout {
                        columns: Theme.isSmall ? 1 : 2
                        columnSpacing: Theme.spacingHuge
                        MetricLarge { label: "FPS"; value: backend.system.fps.toFixed(0); color: Theme.success }
                        MetricLarge { label: "BITRATE"; value: backend.system.bitrate.toFixed(1); suffix: "Mb"; color: Theme.primary }
                        MetricLarge { label: "LATENCY"; value: backend.system.e2eLatency.toFixed(1); suffix: "ms"; color: Theme.accent }
                        MetricLarge { label: "LOSS"; value: (backend.system.lossRate * 100).toFixed(2) + "%"; color: Theme.warning }
                    }
                    Item { Layout.fillWidth: true; visible: !Theme.isSmall }
                    NexusGraph {
                        Layout.preferredWidth: Theme.isSmall ? parent.width : 300
                        Layout.preferredHeight: 120
                        dataModel: backend.system.fpsHistory
                        maxValue: 70; lineWeightColor: Theme.success
                    }
                }
            }
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.isSmall ? 150 : 250
                title: "Control"
                NexusButton {
                    anchors.centerIn: parent
                    text: "Stop Session"
                    onClicked: backend.system.stopSession()
                }
            }
        }

        // SECOND ROW - ADAPTIVE GRID
        GridLayout {
            Layout.fillWidth: true
            columnSpacing: Theme.adaptiveCardSpacing
            rowSpacing: Theme.adaptiveCardSpacing
            columns: Theme.isSmall ? 1 : (Theme.isLarge ? 3 : 2)

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                title: "CPU Usage"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Theme.spacingLarge
                        Item {
                            width: 80; height: 80
                            Canvas {
                                id: cpuCanvas
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.beginPath(); ctx.arc(width/2, height/2, 35, 0, 2*Math.PI);
                                    ctx.strokeStyle = Theme.surfaceSecondary; ctx.lineWidth = 8; ctx.stroke();
                                    ctx.beginPath(); ctx.arc(width/2, height/2, 35, -Math.PI/2, (-Math.PI/2) + (backend.system.cpuUsage/100 * 2*Math.PI));
                                    ctx.strokeStyle = Theme.accent; ctx.lineWidth = 8; ctx.lineCap = "round"; ctx.stroke();
                                }
                                Connections {
                                    target: backend.system
                                    function onCpuUsageChanged() { cpuCanvas.requestPaint() }
                                }
                            }
                            Text { anchors.centerIn: parent; text: backend.system.cpuUsage.toFixed(0) + "%"; font.family: Theme.fontFamily; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.textPrimary }
                        }
                        Column {
                            spacing: 4
                            Text { text: "Average Load"; color: Theme.textSecondary; font.pixelSize: 11 }
                            Text { text: "System Processor"; color: Theme.textPrimary; font.weight: Font.Medium; font.pixelSize: 13 }
                            Text { text: "Status: Optimal"; color: Theme.success; font.pixelSize: 11; font.weight: Font.Bold }
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
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Theme.spacingLarge
                        Item {
                            width: 80; height: 80
                            Canvas {
                                id: memoryCanvas
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.beginPath(); ctx.arc(width/2, height/2, 35, 0, 2*Math.PI);
                                    ctx.strokeStyle = Theme.surfaceSecondary; ctx.lineWidth = 8; ctx.stroke();
                                    ctx.beginPath(); ctx.arc(width/2, height/2, 35, -Math.PI/2, (-Math.PI/2) + (backend.system.memoryUsage/100 * 2*Math.PI));
                                    ctx.strokeStyle = Theme.success; ctx.lineWidth = 8; ctx.lineCap = "round"; ctx.stroke();
                                }
                                Connections {
                                    target: backend.system
                                    function onMemoryUsageChanged() { memoryCanvas.requestPaint() }
                                }
                            }
                            Text { anchors.centerIn: parent; text: backend.system.memoryUsage.toFixed(0) + "%"; font.family: Theme.fontFamily; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.textPrimary }
                        }
                        Column {
                            spacing: 4
                            Text { text: "Used Memory"; color: Theme.textSecondary; font.pixelSize: 11 }
                            Text { text: "System Memory"; color: Theme.textPrimary; font.weight: Font.Medium; font.pixelSize: 13 }
                            Text { text: "Status: Balanced"; color: Theme.success; font.pixelSize: 11 }
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
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    Item { Layout.fillHeight: true }
                    Text { Layout.alignment: Qt.AlignHCenter; text: backend.system.uptime; font.family: Theme.monoFontFamily; font.pixelSize: Theme.isSmall ? 28 : 36; font.weight: Font.Bold; color: Theme.primary }
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
                    ComboBox { model: ["All Levels", "Info", "Warning", "Error"]; Layout.preferredWidth: 150; visible: !Theme.isSmall }
                    Item { Layout.fillWidth: true }
                    NexusButton { text: "Clear"; primary: false; Layout.preferredWidth: 80 }
                    NexusButton { text: "Export"; primary: false; Layout.preferredWidth: 80 }
                }
                NexusTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: [
                        { name: "Level", role: "level", width: 80 },
                        { name: "Event", role: "event", width: Theme.isSmall ? 100 : 150 },
                        { name: "Description", role: "desc", width: Theme.isSmall ? 200 : 400 },
                        { name: "Timestamp", role: "time", width: 150, visible: !Theme.isSmall }
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
            Text { text: value; color: metricRoot.color; font.pixelSize: Theme.isSmall ? 22 : 28; font.weight: Font.Bold; font.family: Theme.monoFontFamily }
            Text { text: suffix; color: Theme.textSecondary; font.pixelSize: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 4 }
        }
    }
}
