import QtQuick
import "../theme"

Item {
    id: root

    property string label: ""
    property string icon: ""
    property bool active: false
    property bool collapsed: false

    signal clicked()

    height: 48
    width: parent.width

    Rectangle {
        anchors.fill: parent
        anchors.margins: Theme.spacingTiny
        radius: Theme.radiusMedium
        color: root.active ? Theme.surfaceSecondary : "transparent"

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.clicked()
            cursorShape: Qt.PointingHandCursor

            onEntered: if(!root.active) parent.color = Qt.rgba(Theme.surfaceSecondary.r, Theme.surfaceSecondary.g, Theme.surfaceSecondary.b, 0.5)
            onExited: if(!root.active) parent.color = "transparent"
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingMedium
            spacing: Theme.spacingMedium

            // Icon Placeholder
            Rectangle {
                width: 20
                height: 20
                radius: 4
                color: root.active ? Theme.accent : Theme.textSecondary

                Text {
                    anchors.centerIn: parent
                    text: root.icon
                    color: "#FFFFFF"
                    font.pixelSize: 12
                }
            }

            Text {
                text: root.label
                visible: !root.collapsed
                font.family: Theme.fontFamily
                font.pixelSize: 14
                font.weight: root.active ? Font.Medium : Font.Normal
                color: root.active ? Theme.textPrimary : Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Active Indicator
        Rectangle {
            width: 3
            height: 20
            radius: 2
            color: Theme.accent
            visible: root.active
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
