import QtQuick
import QtQuick.Controls
import NexusDash

TextField {
    id: control

    font.family: Theme.fontFamily
    font.pixelSize: 14
    color: Theme.textPrimary
    selectionColor: Theme.accent
    selectedTextColor: "#FFFFFF"
    placeholderTextColor: Theme.textSecondary
    verticalAlignment: TextInput.AlignVCenter

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 40
        radius: Theme.radiusMedium
        color: Theme.surfaceSecondary
        border.color: control.activeFocus ? Theme.accent : Theme.border
        border.width: control.activeFocus ? 2 : 1

        Behavior on border.color { ColorAnimation { duration: 150 } }
    }
}
