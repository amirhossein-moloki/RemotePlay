import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme"
import "../components"

ScrollView {
    id: root
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    clip: true

    readonly property bool isActive: backend.system.isSessionActive

    // UI States: Idle, HostingActive, ClientConnected
    state: !isActive ? "Idle" : (backend.system.rtt > 0 ? "ClientConnected" : "HostingActive")

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.adaptiveCardSpacing
        anchors.margins: Theme.spacingTiny

        DashboardHeader {
            title: root.state === "Idle" ? "Dashboard" : (root.state === "HostingActive" ? "Hosting Active" : "Live Session")

            RowLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.spacingMedium
                visible: root.isActive

                // Quality Feedback
                Rectangle {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: tierLabel.implicitWidth + 20
                    radius: 12
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.3)
                    visible: root.state === "ClientConnected"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text { text: "⚡"; font.pixelSize: 10 }
                        Text {
                            id: tierLabel
                            text: backend.system.currentQualityTier
                            color: Theme.accent
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            font.family: Theme.fontFamily
                        }
                    }
                }

                NetworkStatusIndicator {
                    latency: backend.system.e2eLatency
                    loss: backend.system.lossRate
                }

                NexusButton {
                    text: "Stop Session"
                    Layout.preferredHeight: 32
                    onClicked: backend.system.stopSession()
                }
            }
        }

        // --- IDLE STATE ---
        GridLayout {
            id: idleGrid
            Layout.fillWidth: true
            columnSpacing: Theme.adaptiveCardSpacing
            rowSpacing: Theme.adaptiveCardSpacing
            columns: Theme.isSmall ? 1 : 2
            visible: root.state === "Idle"

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 400
                title: "Start Hosting"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium

                    Text {
                        text: "Allow others to connect and play on your system."
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSmall
                        Text { text: "Network Interface"; color: Theme.textSecondary; font.pixelSize: 11; font.weight: Font.Bold }
                        NexusComboBox {
                            id: interfaceCombo
                            Layout.fillWidth: true
                            model: backend.system.networkInterfaces
                        }
                    }

                    Item { Layout.fillHeight: true }

                    NexusButton {
                        text: "Go Live"
                        Layout.fillWidth: true
                        onClicked: backend.system.startHost(interfaceCombo.currentText, 5000, 60)
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 400
                title: "Connect to Host"

                property bool isConnecting: false

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    visible: !parent.isConnecting

                    NexusInput {
                        id: hostIpInput
                        placeholderText: "Remote Host IP (e.g. 192.168.1.XX)"
                        Layout.fillWidth: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingTiny
                        Text { text: "Recent Connections"; color: Theme.textSecondary; font.pixelSize: 11; font.weight: Font.Bold }
                        ListView {
                            Layout.fillWidth: true
                            height: 120
                            clip: true
                            model: backend.system.recentHosts
                            delegate: Item {
                                width: parent.width; height: 36
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: 2; radius: 6; color: Theme.surfaceSecondary
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12; anchors.rightMargin: 12
                                        Text { text: modelData; color: Theme.textPrimary; font.pixelSize: 12; Layout.fillWidth: true }
                                        Text { text: "Saved"; color: Theme.success; font.pixelSize: 10; font.weight: Font.Bold }
                                    }
                                    MouseArea { anchors.fill: parent; onClicked: hostIpInput.text = modelData; cursorShape: Qt.PointingHandCursor }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: parent.count === 0
                                text: "No history yet"
                                color: Theme.textSecondary
                                font.pixelSize: 11
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
                                hostIpInput.errorMessage = "Enter a valid IP (e.g. 192.168.1.10)"
                                return
                            }
                            hostIpInput.isInvalid = false
                            parent.isConnecting = true
                            backend.system.startClient(interfaceCombo.currentText, hostIpInput.text, 5000, 60)
                        }
                    }
                }

                // Connecting State Overlay
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Theme.spacingMedium
                    visible: parent.isConnecting

                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        running: parent.visible
                    }

                    Text {
                        text: "Establishing Secure Connection..."
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Attempting handshake with " + hostIpInput.text
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }

                    NexusButton {
                        text: "Cancel"
                        primary: false
                        Layout.alignment: Qt.AlignHCenter
                        onClicked: {
                            backend.system.stopSession()
                            parent.parent.isConnecting = false
                        }
                    }
                }

                Connections {
                    target: backend.system
                    function onSessionStateChanged() {
                        if (!backend.system.isSessionActive) {
                            parent.isConnecting = false
                        }
                    }
                }
            }
        }

        // --- HOSTING ACTIVE STATE ---
        NexusCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            visible: root.state === "HostingActive"
            title: "Host Information"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingLarge

                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Theme.spacingSmall
                    Text { text: "YOUR IP ADDRESS"; color: Theme.textSecondary; font.pixelSize: 11; font.weight: Font.Bold; Layout.alignment: Qt.AlignHCenter }
                    RowLayout {
                        spacing: Theme.spacingMedium
                        Text {
                            text: backend.system.getIpFromInterface(interfaceCombo.currentText)
                            font.pixelSize: 32
                            font.family: Theme.monoFontFamily
                            font.weight: Font.Bold
                            color: Theme.primary
                        }
                        NexusButton {
                            text: "Copy"
                            primary: false
                            Layout.preferredHeight: 32
                            onClicked: {
                                var ip = backend.system.getIpFromInterface(interfaceCombo.currentText);
                                backend.system.copyToClipboard(ip);
                                globalNotification.show("IP Copied to Clipboard: " + ip, "Success")
                            }
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                Text {
                    text: "Waiting for incoming connections..."
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                    font.italic: true
                }
            }
        }

        // --- CLIENT CONNECTED STATE (CONTROL CENTER) ---
        GridLayout {
            Layout.fillWidth: true
            columns: Theme.isSmall ? 1 : 4
            columnSpacing: Theme.adaptiveCardSpacing
            visible: root.state === "ClientConnected"

            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                MetricLarge { label: "LATENCY"; value: backend.system.e2eLatency.toFixed(1); suffix: "ms"; color: Theme.accent; anchors.centerIn: parent }
            }
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                MetricLarge { label: "FPS"; value: backend.system.fps.toFixed(0); suffix: "fps"; color: Theme.success; anchors.centerIn: parent }
            }
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                MetricLarge { label: "BITRATE"; value: backend.system.bitrate.toFixed(1); suffix: "Mbps"; color: Theme.primary; anchors.centerIn: parent }
            }
            NexusCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                MetricLarge { label: "LOSS"; value: (backend.system.lossRate * 100).toFixed(2); suffix: "%"; color: Theme.danger; anchors.centerIn: parent }
            }
        }

        NexusCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 250
            visible: root.state === "ClientConnected"
            title: "Performance History"

            NexusGraph {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                dataModel: backend.system.fpsHistory
                maxValue: 70
                lineWeightColor: Theme.success
            }
        }

        // --- SYSTEM METRICS (ALWAYS VISIBLE) ---
        GridLayout {
            Layout.fillWidth: true
            columnSpacing: Theme.adaptiveCardSpacing
            rowSpacing: Theme.adaptiveCardSpacing
            columns: Theme.isSmall ? 1 : (Theme.isLarge ? 3 : 2)

            NexusCard {
                Layout.fillWidth: true; Layout.preferredHeight: 280
                title: "CPU Usage"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingMedium
                    MetricCircular { value: backend.system.cpuUsage; color: Theme.accent; label: "CPU Load" }
                    NexusGraph { Layout.fillWidth: true; Layout.preferredHeight: 60; dataModel: backend.system.cpuHistory; maxValue: 100; lineWeightColor: Theme.accent }
                }
            }
            NexusCard {
                Layout.fillWidth: true; Layout.preferredHeight: 280
                title: "Memory Usage"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingMedium
                    MetricCircular { value: backend.system.memoryUsage; color: Theme.success; label: "RAM Used" }
                    NexusGraph { Layout.fillWidth: true; Layout.preferredHeight: 60; dataModel: backend.system.memoryHistory; maxValue: 100; lineWeightColor: Theme.success }
                }
            }
            NexusCard {
                Layout.fillWidth: true; Layout.preferredHeight: 280
                title: "System Uptime"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingMedium
                    Item { Layout.fillHeight: true }
                    Text { Layout.alignment: Qt.AlignHCenter; text: backend.system.uptime; font.family: Theme.monoFontFamily; font.pixelSize: 32; font.weight: Font.Bold; color: Theme.primary }
                    Text { Layout.alignment: Qt.AlignHCenter; text: "SYSTEM ACTIVE TIME"; font.pixelSize: 10; font.weight: Font.Black; color: Theme.textSecondary; font.letterSpacing: 1 }
                    Item { Layout.fillHeight: true }
                    NexusGraph { Layout.fillWidth: true; Layout.preferredHeight: 40; dataModel: backend.system.latencyHistory; maxValue: 100; lineWeightColor: Theme.success; fillEnabled: false }
                }
            }
        }
    }

    // --- REUSABLE COMPONENTS ---
    component MetricLarge : Column {
        id: metricRoot
        property string label: ""
        property string value: ""
        property string suffix: ""
        property color color: Theme.textPrimary
        spacing: 2
        Text { text: label; color: Theme.textSecondary; font.pixelSize: 11; font.weight: Font.Bold; anchors.horizontalCenter: parent.horizontalCenter }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 4
            Text { text: value; color: metricRoot.color; font.pixelSize: 32; font.weight: Font.Bold; font.family: Theme.monoFontFamily }
            Text { text: suffix; color: Theme.textSecondary; font.pixelSize: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 6 }
        }
    }

    component MetricCircular : RowLayout {
        property real value: 0
        property color color: Theme.primary
        property string label: ""
        spacing: Theme.spacingLarge
        Layout.alignment: Qt.AlignHCenter

        Item {
            width: 80; height: 80
            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.beginPath(); ctx.arc(width/2, height/2, 35, 0, 2*Math.PI);
                    ctx.strokeStyle = Theme.surfaceSecondary; ctx.lineWidth = 8; ctx.stroke();
                    ctx.beginPath(); ctx.arc(width/2, height/2, 35, -Math.PI/2, (-Math.PI/2) + (value/100 * 2*Math.PI));
                    ctx.strokeStyle = color; ctx.lineWidth = 8; ctx.lineCap = "round"; ctx.stroke();
                }
                Connections { target: backend.system; function onCpuUsageChanged() { parent.requestPaint() } function onMemoryUsageChanged() { parent.requestPaint() } }
            }
            Text { anchors.centerIn: parent; text: value.toFixed(0) + "%"; font.family: Theme.fontFamily; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.textPrimary }
        }
        Column {
            Text { text: label; color: Theme.textSecondary; font.pixelSize: 12 }
            Text { text: value > 80 ? "High Load" : "Optimal"; color: value > 80 ? Theme.danger : Theme.success; font.weight: Font.Bold; font.pixelSize: 14 }
        }
    }
}
