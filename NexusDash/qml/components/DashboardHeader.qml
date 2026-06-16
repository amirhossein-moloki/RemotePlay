import QtQuick
import QtQuick.Layouts
import NexusDash

Item {
    id: root
    height: 100
    Layout.fillWidth: true

    property string title: "Dashboard"
    property string subtitle: "Monitor and manage your remote streaming environment"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        spacing: Theme.spacingMedium

        ColumnLayout {
            spacing: Theme.spacingTiny
            Layout.fillWidth: true

            Text {
                text: root.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.isSmall ? 24 : 32
                font.weight: Font.Bold
                color: Theme.textPrimary
            }

            Text {
                text: root.subtitle
                visible: !Theme.isSmall
                font.family: Theme.fontFamily
                font.pixelSize: 14
                color: Theme.textSecondary
            }
        }

        // Connection Status Indicator
        Rectangle {
            Layout.preferredWidth: Theme.isSmall ? 120 : 160
            Layout.preferredHeight: 40
            color: Theme.surface
            radius: Theme.radiusMedium
            border.color: Theme.border
            border.width: 1

            RowLayout {
                anchors.centerIn: parent
                spacing: Theme.spacingSmall

                // Animated Status Dot
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: backend.system.isSessionActive ? Theme.success : Theme.danger

                    SequentialAnimation on opacity {
                        id: dotAnim
                        loops: Animation.Infinite
                        running: backend.system.isSessionActive
                        NumberAnimation { from: 1.0; to: 0.4; duration: 800; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 0.4; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                        onRunningChanged: if (!running) parent.opacity = 1.0
                    }
                }

                Text {
                    text: backend.system.isSessionActive ? "Connected" : "Disconnected"
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }
            }
        }

        // Date/Time Section
        Column {
            Layout.preferredWidth: Theme.isSmall ? 80 : 120
            Layout.alignment: Qt.AlignVCenter
            visible: !Theme.isSmall

            Text {
                id: timeText
                anchors.right: parent.right
                font.family: Theme.fontFamily
                font.pixelSize: 18
                font.weight: Font.Bold
                color: Theme.textPrimary
                text: Qt.formatTime(new Date(), "hh:mm:ss")
            }

            Text {
                anchors.right: parent.right
                font.family: Theme.fontFamily
                font.pixelSize: 12
                color: Theme.textSecondary
                text: Qt.formatDate(new Date(), "ddd, MMM d")
            }

            Timer {
                interval: 1000
                running: true
                repeat: true
                onTriggered: {
                    timeText.text = Qt.formatTime(new Date(), "hh:mm:ss")
                }
            }
        }
    }
}
