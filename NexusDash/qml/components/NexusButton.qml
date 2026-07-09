import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

Button {
    id: control

    property bool primary: true
    property bool loading: false
    property string iconSource: ""

    contentItem: Row {
        spacing: Theme.spacingSmall
        anchors.centerIn: parent

        Item {
            visible: control.loading
            width: 16
            height: 16

            RotationAnimation on rotation {
                running: control.loading
                from: 0; to: 360; duration: 1000; loops: Animation.Infinite
            }

            Rectangle {
                anchors.fill: parent
                radius: 8
                color: "transparent"
                border.color: control.primary ? "white" : Theme.primary
                border.width: 2
                clip: true
                Rectangle {
                    width: 16; height: 8; color: control.primary ? Theme.primary : Theme.panel; anchors.top: parent.top
                }
            }
        }

        Text {
            visible: !control.loading && control.iconSource !== ""
            text: control.iconSource
            font.family: Theme.fontFamily
            font.pixelSize: 16
            color: control.primary ? "#FFFFFF" : Theme.textPrimary
        }

        Text {
            text: control.text
            font.family: Theme.fontFamily
            font.pixelSize: 14
            font.weight: Font.Medium
            color: control.primary ? "#FFFFFF" : Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 40
        radius: Theme.radiusMedium
        color: control.primary ?
               (control.down ? Theme.primary : (control.hovered ? Theme.primaryHover : Theme.primary)) :
               (control.down ? Theme.surfaceSecondary : (control.hovered ? Qt.lighter(Theme.panel, 1.2) : "transparent"))
        border.color: control.primary ? "transparent" : Theme.border
        border.width: 1

        opacity: (control.enabled && !control.loading) ? 1.0 : 0.6

        Behavior on color { ColorAnimation { duration: 150 } }

        // Premium Glow for primary button
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Theme.primary
            opacity: 0.15
            visible: control.primary && control.hovered
            scale: 1.04
            z: -1
            Behavior on scale { NumberAnimation { duration: 200 } }
        }
    }

    scale: control.pressed ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: 100 } }
}
