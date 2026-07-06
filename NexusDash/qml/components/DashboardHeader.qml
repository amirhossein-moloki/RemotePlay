import QtQuick
import QtQuick.Layouts
import "../theme"

Item {
    id: root
    height: 80
    Layout.fillWidth: true

    property string title: "Dashboard"
    property string subtitle: "Monitor and manage your remote streaming environment"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        spacing: Theme.spacingMedium

        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true

            Text {
                text: root.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeH2
                font.weight: Font.Bold
                color: Theme.textPrimary
            }

            Text {
                text: root.subtitle
                visible: !Theme.isSmall
                font.family: Theme.fontFamily
                font.pixelSize: 13
                color: Theme.textSecondary
            }
        }

        // Connection Status Indicator (Compact)
        Rectangle {
            Layout.preferredWidth: 140
            Layout.preferredHeight: 36
            color: Theme.panel
            radius: Theme.radiusMedium
            border.color: Theme.border
            border.width: 1

            RowLayout {
                anchors.centerIn: parent
                spacing: Theme.spacingSmall

                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: statusColor
                }

                Text {
                    text: statusText
                    font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.Bold; color: Theme.textPrimary
                }
            }
        }

        readonly property int appStatus: backend.system.appStatus
        readonly property color statusColor: {
            switch(appStatus) {
                case 1: return Theme.active;
                case 2: return Theme.success;
                case 3: return Theme.danger;
                case 4: return Theme.warning;
                default: return Theme.textSecondary;
            }
        }
        readonly property string statusText: {
            switch(appStatus) {
                case 1: return "Hosting";
                case 2: return "Connected";
                case 3: return "Error";
                case 4: return "Connecting";
                default: return "Idle";
            }
        }
    }
}
