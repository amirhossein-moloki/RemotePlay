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

    readonly property int appStatus: backend.system.appStatus

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacingLarge
        anchors.margins: Theme.adaptiveMargin

        // --- 1. GLOBAL CONNECTION STATUS (TOP) ---
        NexusCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 100

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingLarge

                // Animated Status Indicator
                Rectangle {
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    radius: 32
                    color: Qt.rgba(statusColor.r, statusColor.g, statusColor.b, 0.1)

                    Rectangle {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        radius: 8
                        color: statusColor

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: root.appStatus !== 0 // Idle
                            NumberAnimation { from: 1.0; to: 0.4; duration: 1000; easing.type: Easing.InOutQuad }
                            NumberAnimation { from: 0.4; to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        text: statusTitle
                        font.family: Theme.fontFamily
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: statusSubtitle
                        font.family: Theme.fontFamily
                        font.pixelSize: 13
                        color: Theme.textSecondary
                    }
                }

                RowLayout {
                    spacing: Theme.spacingMedium
                    visible: root.appStatus === 2 // Connected

                    NetworkStatusIndicator {
                        latency: backend.system.e2eLatency
                        loss: backend.system.lossRate
                    }
                }

                NexusButton {
                    text: "End Session"
                    visible: root.appStatus === 1 || root.appStatus === 2 || root.appStatus === 4 // Hosting, Connected, Reconnecting
                    onClicked: backend.system.stopSession()
                }
            }

            readonly property color statusColor: {
                switch(root.appStatus) {
                    case 1: return Theme.active;     // Hosting (Blue)
                    case 2: return Theme.success;    // Connected (Green)
                    case 3: return Theme.danger;     // Error (Red)
                    case 4: return Theme.warning;    // Reconnecting/Connecting (Yellow)
                    default: return Theme.textSecondary; // Idle
                }
            }

            readonly property string statusTitle: {
                switch(root.appStatus) {
                    case 1: return "Hosting Active";
                    case 2: return "Live Session Active";
                    case 3: return "System Error";
                    case 4: return "Establishing Connection...";
                    default: return "System Idle";
                }
            }

            readonly property string statusSubtitle: {
                switch(root.appStatus) {
                    case 1: return "Your machine is ready for incoming connections.";
                    case 2: return "Connected to remote host. Stream is live.";
                    case 3: return "A critical error occurred. Check logs for details.";
                    case 4: return "Handshake in progress. Resolving network...";
                    default: return "Start a new session or connect to a remote host.";
                }
            }
        }

        // --- 2. SESSION CONTROLS (MIDDLE) ---

        // Connection Progress Indicator (only visible when connecting)
        NexusCard {
            Layout.fillWidth: true
            visible: root.appStatus === 4 // Reconnecting/Connecting
            title: "Connection Pipeline"

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: 0

                Repeater {
                    model: ["Resolving", "Handshake", "Encrypting", "Connected"]
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            width: 24; height: 24; radius: 12
                            color: backend.system.connectionStep >= index + 1 ? Theme.success : Theme.surface
                            Text {
                                anchors.centerIn: parent
                                text: backend.system.connectionStep > index + 1 ? "✓" : (index + 1)
                                color: "white"; font.pixelSize: 10; font.weight: Font.Bold
                            }
                        }

                        Text {
                            text: modelData
                            color: backend.system.connectionStep >= index + 1 ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: 12; font.weight: backend.system.connectionStep === index + 1 ? Font.Bold : Font.Normal
                        }

                        Rectangle {
                            visible: index < 3
                            Layout.fillWidth: true; height: 2; color: backend.system.connectionStep > index + 1 ? Theme.success : Theme.border
                            Layout.leftMargin: 8; Layout.rightMargin: 16
                        }
                    }
                }
            }
        }

        // Action Cards (Idle State)
        GridLayout {
            Layout.fillWidth: true
            columns: Theme.isSmall ? 1 : 2
            columnSpacing: Theme.adaptiveCardSpacing
            visible: root.appStatus === 0 || root.appStatus === 3 // Idle or Error

            NexusCard {
                Layout.fillWidth: true; Layout.preferredHeight: 320
                title: "Host Session"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingLarge; spacing: Theme.spacingMedium
                    Text {
                        text: "Turn this machine into a high-performance streaming host."
                        color: Theme.textSecondary; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Text { text: "NETWORK INTERFACE"; color: Theme.textSecondary; font.pixelSize: 10; font.weight: Font.Black; font.letterSpacing: 1 }
                        NexusComboBox { id: interfaceCombo; Layout.fillWidth: true; model: backend.system.networkInterfaces }
                    }

                    Item { Layout.fillHeight: true }
                    NexusButton {
                        text: "Start Hosting Session"; Layout.fillWidth: true; primary: true
                        onClicked: backend.system.startHost(interfaceCombo.currentText, 5000, 60)
                    }
                }
            }

            NexusCard {
                Layout.fillWidth: true; Layout.preferredHeight: 320
                title: "Connect to Host"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingLarge; spacing: Theme.spacingMedium
                    NexusInput { id: hostIpInput; placeholderText: "Remote Host IP (e.g. 192.168.1.50)"; Layout.fillWidth: true }

                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 8
                        Text { text: "RECENT CONNECTIONS"; color: Theme.textSecondary; font.pixelSize: 10; font.weight: Font.Black; font.letterSpacing: 1 }
                        RowLayout {
                            spacing: 8
                            Repeater {
                                model: backend.system.recentHosts
                                delegate: NexusButton {
                                    text: modelData; primary: false; Layout.preferredHeight: 32
                                    onClicked: hostIpInput.text = modelData
                                }
                            }
                            Text { text: "No history"; visible: backend.system.recentHosts.length === 0; color: Theme.textSecondary; font.pixelSize: 12 }
                        }
                    }

                    Item { Layout.fillHeight: true }
                    NexusButton {
                        text: "Connect to Host"; Layout.fillWidth: true; primary: true
                        loading: root.appStatus === 4
                        onClicked: {
                            if (hostIpInput.text === "") {
                                hostIpInput.isInvalid = true
                                return
                            }
                            backend.system.startClient(interfaceCombo.currentText, hostIpInput.text, 5000, 60)
                        }
                    }
                }
            }
        }

        // Hosting Active Info
        NexusCard {
            Layout.fillWidth: true
            visible: root.appStatus === 1 // Hosting
            title: "Host Configuration"

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingExtraHuge

                ColumnLayout {
                    spacing: 4
                    Text { text: "LOCAL ACCESS IP"; color: Theme.textSecondary; font.pixelSize: 10; font.weight: Font.Black }
                    Text {
                        text: backend.system.getIpFromInterface(interfaceCombo.currentText)
                        font.family: Theme.monoFontFamily; font.pixelSize: 32; font.weight: Font.Bold; color: Theme.primary
                    }
                }

                ColumnLayout {
                    spacing: 8
                    Text { text: "Share this IP with clients on your network."; color: Theme.textSecondary; font.pixelSize: 13 }
                    RowLayout {
                        spacing: 12
                        NexusButton {
                            text: "Copy Invite"; primary: true; iconSource: "📋"
                            onClicked: {
                                var ip = backend.system.getIpFromInterface(interfaceCombo.currentText);
                                backend.system.copyToClipboard(ip);
                                globalNotification.show("Host IP copied to clipboard", "Success")
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        // --- 3. SYSTEM MONITORING (BOTTOM) ---
        Text {
            text: "System Telemetry"
            font.family: Theme.fontFamily; font.pixelSize: 18; font.weight: Font.Bold; color: Theme.textPrimary
            Layout.topMargin: Theme.spacingMedium
        }

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: Theme.spacingLarge
            rowSpacing: Theme.spacingLarge
            columns: Theme.isSmall ? 1 : 3

            // CPU Monitor
            NexusCard {
                Layout.fillWidth: true; Layout.preferredHeight: 200
                title: "Processor Load"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingSmall
                    RowLayout {
                        Text { text: backend.system.cpuUsage.toFixed(1) + "%"; font.family: Theme.monoFontFamily; font.pixelSize: 24; font.weight: Font.Bold; color: Theme.textPrimary }
                        Item { Layout.fillWidth: true }
                        StatusBadge { value: backend.system.cpuUsage; labels: ["OPTIMAL", "HIGH", "CRITICAL"]; thresholds: [70, 90] }
                    }
                    NexusGraph { Layout.fillWidth: true; Layout.fillHeight: true; dataModel: backend.system.cpuHistory; maxValue: 100; lineWeightColor: Theme.primary }
                }
            }

            // RAM Monitor
            NexusCard {
                Layout.fillWidth: true; Layout.preferredHeight: 200
                title: "Memory Usage"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingSmall
                    RowLayout {
                        Text { text: backend.system.memoryUsage.toFixed(1) + "%"; font.family: Theme.monoFontFamily; font.pixelSize: 24; font.weight: Font.Bold; color: Theme.textPrimary }
                        Item { Layout.fillWidth: true }
                        StatusBadge { value: backend.system.memoryUsage; labels: ["GOOD", "HEAVY", "CRITICAL"]; thresholds: [75, 90] }
                    }
                    NexusGraph { Layout.fillWidth: true; Layout.fillHeight: true; dataModel: backend.system.memoryHistory; maxValue: 100; lineWeightColor: Theme.success }
                }
            }

            // Network/Uptime Monitor
            NexusCard {
                Layout.fillWidth: true; Layout.preferredHeight: 200
                title: "Session Analytics"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.spacingMedium; spacing: Theme.spacingMedium

                    ColumnLayout {
                        spacing: 2
                        Text { text: "APP UPTIME"; color: Theme.textSecondary; font.pixelSize: 10; font.weight: Font.Black }
                        Text { text: backend.system.uptime; font.family: Theme.monoFontFamily; font.pixelSize: 16; color: Theme.textPrimary }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                    ColumnLayout {
                        spacing: 2
                        Text { text: "SESSION UPTIME"; color: Theme.textSecondary; font.pixelSize: 10; font.weight: Font.Black }
                        Text {
                            text: backend.system.sessionUptime
                            font.family: Theme.monoFontFamily; font.pixelSize: 16
                            color: root.appStatus === 0 ? Theme.textSecondary : Theme.success
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Theme.spacingHuge }
    }

    // Helper for status badges
    component StatusBadge : Rectangle {
        property real value: 0
        property var labels: []
        property var thresholds: []

        width: badgeText.implicitWidth + 16; height: 22; radius: 11
        color: value > thresholds[1] ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.1) :
               (value > thresholds[0] ? Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.1) :
                                       Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.1))

        Text {
            id: badgeText
            anchors.centerIn: parent
            text: value > thresholds[1] ? labels[2] : (value > thresholds[0] ? labels[1] : labels[0])
            font.pixelSize: 9; font.weight: Font.Black; font.letterSpacing: 0.5
            color: value > thresholds[1] ? Theme.danger : (value > thresholds[0] ? Theme.warning : Theme.success)
        }
    }
}
