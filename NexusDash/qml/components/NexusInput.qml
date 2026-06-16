import QtQuick
import QtQuick.Controls
import NexusDash

TextField {
    id: control

    property bool isInvalid: false
    property string errorMessage: ""

    font.family: Theme.fontFamily
    font.pixelSize: 14
    color: Theme.textPrimary
    selectionColor: Theme.accent
    selectedTextColor: "#FFFFFF"
    placeholderTextColor: Theme.textSecondary
    verticalAlignment: TextInput.AlignVCenter

    leftPadding: Theme.spacingMedium
    rightPadding: Theme.spacingMedium

    background: Column {
        Rectangle {
            implicitWidth: control.width
            implicitHeight: 36
            radius: Theme.radiusMedium
            color: Theme.surfaceSecondary
            border.color: control.isInvalid ? Theme.accent : (control.activeFocus ? Theme.primary : Theme.border)
            border.width: (control.activeFocus || control.isInvalid) ? 2 : 1

            Behavior on border.color { ColorAnimation { duration: 150 } }
        }

        Text {
            text: control.errorMessage
            color: Theme.accent
            font.pixelSize: 10
            font.family: Theme.fontFamily
            visible: control.isInvalid
            width: control.width
            wrapMode: Text.WordWrap
        }
    }
}
