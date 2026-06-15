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

        NexusCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 450

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.spacingHuge
                width: parent.width * 0.7

                Rectangle {
                    width: 100; height: 100; radius: 24
                    color: Theme.primary
                    Layout.alignment: Qt.AlignHCenter

                    Text {
                        anchors.centerIn: parent
                        text: "N"
                        color: "white"
                        font.bold: true
                        font.pixelSize: 48
                    }

                    // Glow effect
                    layer.enabled: true
                    layer.effect: ShaderEffect {
                        // In a real environment, we'd use a DropShadow or similar
                    }
                }

                Column {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall
                    Text {
                        text: "NexusDash Professional"
                        font.family: Theme.fontFamily
                        font.pixelSize: 32
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "Version 1.0.0 Stable Build"
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                        color: Theme.accent
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                Text {
                    text: "A commercial-grade LAN streaming orchestrator powered by the ParsecLite Core Engine. Engineered for ultra-low latency, professional monitoring, and enterprise-level stability."
                    font.family: Theme.fontFamily
                    font.pixelSize: 16
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.4
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Theme.spacingMedium

                    NexusButton {
                        text: "Documentation"
                        primary: true
                    }

                    NexusButton {
                        text: "Check for Updates"
                        primary: false
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingTiny

            Text {
                text: "© 2025 NexusDash Systems Inc. All rights reserved."
                font.family: Theme.fontFamily
                font.pixelSize: 12
                color: Theme.textSecondary
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "ParsecLite Core v2.4.5-stable | D3D11 | NVENC | FFmpeg 6.0"
                font.family: Theme.fontFamily
                font.pixelSize: 10
                color: Qt.rgba(1,1,1,0.2)
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
