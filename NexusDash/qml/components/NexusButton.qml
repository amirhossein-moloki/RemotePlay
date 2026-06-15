import QtQuick
import QtQuick.Controls
import "../theme"

Button {
    id: control

    property bool primary: true
    property string icon: ""

    contentItem: Row {
        spacing: Theme.spacingSmall
        alignment: Qt.AlignHCenter

        Text {
            visible: control.icon !== ""
            text: control.icon
            font.family: Theme.fontFamily
            font.pixelSize: 16
            color: control.primary ? "#FFFFFF" : Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            text: control.text
            font.family: Theme.fontFamily
            font.pixelSize: 14
            font.weight: Font.Medium
            color: control.primary ? "#FFFFFF" : Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 44
        radius: Theme.radiusMedium
        color: {
            if (!control.enabled) return Theme.panel;
            if (control.primary) {
                return control.down ? Qt.darker(Theme.primary, 1.1) : (control.hovered ? Theme.accentHover : Theme.primary)
            }
            return control.down ? Theme.panel : (control.hovered ? Qt.rgba(1,1,1,0.05) : "transparent")
        }
        border.color: control.primary ? "transparent" : Theme.border
        border.width: 1

        Behavior on color { ColorAnimation { duration: 150 } }

        scale: control.pressed ? 0.97 : 1.0
        Behavior on scale { NumberAnimation { duration: 100 } }
    }

    cursorShape: Qt.PointingHandCursor
}
