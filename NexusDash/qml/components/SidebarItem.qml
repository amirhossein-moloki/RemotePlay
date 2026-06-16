import QtQuick
import NexusDash

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
            anchors.leftMargin: Theme.isSmall && !root.collapsed ? Theme.spacingSmall : Theme.spacingMedium
            spacing: Theme.spacingMedium

            // Icon Container
            Rectangle {
                width: 32
                height: 32
                radius: 8
                color: root.active ? Theme.primary : "transparent"
                border.color: root.active ? "transparent" : Theme.border
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: root.icon
                    color: root.active ? "#FFFFFF" : Theme.textSecondary
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }

                Behavior on color { ColorAnimation { duration: 200 } }
            }

            Text {
                text: root.label
                visible: !root.collapsed
                font.family: Theme.fontFamily
                font.pixelSize: 14
                font.weight: root.active ? Font.DemiBold : Font.Normal
                color: root.active ? Theme.textPrimary : Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter

                Behavior on color { ColorAnimation { duration: 200 } }
            }
        }

        // Active Indicator (Vertical Bar)
        Rectangle {
            width: 4
            height: 24
            radius: 2
            color: Theme.accent
            visible: root.active
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            // Subtle glow
            layer.enabled: true
        }
    }
}
