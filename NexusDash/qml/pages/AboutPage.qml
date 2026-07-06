import QtQuick
import QtQuick.Layouts
import "../theme"
import "../components"

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.adaptiveMargin
        spacing: Theme.adaptiveSpacing

        Text {
            text: "About NexusDash"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.isSmall ? 24 : 28
            font.weight: Font.Bold
            color: Theme.textPrimary
        }

        NexusCard {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.isSmall ? 400 : 350

            Column {
                anchors.centerIn: parent
                spacing: Theme.spacingLarge
                width: parent.width * 0.9

                Rectangle {
                    width: Theme.isSmall ? 60 : 80
                    height: Theme.isSmall ? 60 : 80
                    radius: 20
                    color: Theme.accent
                    anchors.horizontalCenter: parent.horizontalCenter
                    Text {
                        anchors.centerIn: parent
                        text: "N"
                        color: "white"
                        font.bold: true
                        font.pixelSize: Theme.isSmall ? 30 : 40
                    }
                }

                Text {
                    text: "NexusDash v1.0.0"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.isSmall ? 20 : 24
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "A modern, high-performance dashboard template built with Qt 6 and C++. Designed for scalability, ease of use, and a premium user experience."
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.isSmall ? 14 : 16
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                }

                NexusButton {
                    text: "Check for Updates"
                    primary: false
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        Text {
            text: "© 2025 NexusDash Systems Inc."
            font.family: Theme.fontFamily
            font.pixelSize: 12
            color: Theme.textSecondary
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
