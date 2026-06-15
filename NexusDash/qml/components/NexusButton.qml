import QtQuick
import QtQuick.Controls
import NexusDash

Button {
    id: control

    property bool primary: true

    contentItem: Text {
        text: control.text
        font.family: Theme.fontFamily
        font.pixelSize: 14
        font.weight: Font.Medium
        color: control.primary ? "#FFFFFF" : Theme.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 40
        radius: Theme.radiusMedium
        color: control.primary ?
               (control.down ? Theme.primary : (control.hovered ? Theme.primaryHover : Theme.primary)) :
               (control.down ? Theme.surfaceSecondary : (control.hovered ? Qt.lighter(Theme.surface, 1.2) : "transparent"))
        border.color: control.primary ? "transparent" : Theme.border
        border.width: 1

        opacity: control.enabled ? 1.0 : 0.5

        Behavior on color { ColorAnimation { duration: 150 } }

        // Premium Glow for primary button
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Theme.primary
            opacity: 0.2
            visible: control.primary && control.hovered
            scale: 1.05
            z: -1
            Behavior on scale { NumberAnimation { duration: 200 } }
        }
    }

    scale: control.pressed ? 0.98 : 1.0
    Behavior on scale { NumberAnimation { duration: 100 } }
}
