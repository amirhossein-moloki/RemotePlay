import QtQuick
import QtQuick.Controls
import "../theme"

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
               (control.hovered ? Theme.accentHover : Theme.accent) :
               (control.hovered ? Theme.surfaceSecondary : Theme.surface)
        border.color: control.primary ? "transparent" : Theme.border
        border.width: 1

        Behavior on color { ColorAnimation { duration: 150 } }
    }
}
