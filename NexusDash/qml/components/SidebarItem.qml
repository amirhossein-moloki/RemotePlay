import QtQuick
import "../theme"

Item {
    id: root

    property string label: ""
    property string icon: ""
    property bool active: false
    property bool collapsed: false

    signal clicked()

    height: 52
    width: parent.width

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingSmall
        anchors.rightMargin: Theme.spacingSmall
        radius: Theme.radiusMedium
        color: root.active ? Qt.rgba(1,1,1,0.08) : (itemMouseArea.containsMouse ? Qt.rgba(1,1,1,0.04) : "transparent")

        Behavior on color { ColorAnimation { duration: 150 } }

        MouseArea {
            id: itemMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.clicked()
            cursorShape: Qt.PointingHandCursor
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: root.collapsed ? (parent.width - 24) / 2 : Theme.spacingMedium
            spacing: Theme.spacingMedium

            Text {
                text: root.icon
                font.family: "Material Design Icons" // Or placeholder icon font
                font.pixelSize: 20
                color: root.active ? Theme.accent : Theme.textSecondary
                width: 24
                horizontalAlignment: Text.AlignHCenter

                // Temporary fallback if font not loaded
                Rectangle {
                    visible: parent.text === ""
                    width: 12; height: 12; radius: 6
                    color: parent.color
                    anchors.centerIn: parent
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

                opacity: root.collapsed ? 0 : 1
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }

        // Active Indicator (Fluent Style)
        Rectangle {
            width: 3
            height: 16
            radius: 1.5
            color: Theme.accent
            visible: root.active
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
