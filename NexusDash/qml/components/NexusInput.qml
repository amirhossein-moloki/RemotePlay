import QtQuick
import QtQuick.Controls
import "../theme"

TextField {
    id: control

    font.family: Theme.fontFamily
    font.pixelSize: 14
    color: Theme.textPrimary
    selectionColor: Theme.primary
    selectedTextColor: "#FFFFFF"
    placeholderTextColor: Theme.textSecondary
    verticalAlignment: TextInput.AlignVCenter
    leftPadding: Theme.spacingMedium
    rightPadding: Theme.spacingMedium

    background: Rectangle {
        implicitWidth: 240
        implicitHeight: 44
        radius: Theme.radiusMedium
        color: Theme.panel
        border.color: control.activeFocus ? Theme.primary : Theme.border
        border.width: control.activeFocus ? 2 : 1

        Behavior on border.color { ColorAnimation { duration: 150 } }

        // Animated bottom line like WinUI
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            height: 2
            width: control.activeFocus ? parent.width : 0
            color: Theme.accent
            radius: 1
            Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
        }
    }
}
