import QtQuick
import QtQuick.Controls
import ParsecLite.UI

Button {
    id: control

    property color backgroundColor: Style.accent
    property color textColor: "white"

    contentItem: Text {
        text: control.text
        font: Style.bodyFont
        font.bold: true
        color: control.enabled ? rootTextColor() : Style.textSecondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 36
        radius: Style.radius
        color: control.down ? Qt.darker(control.backgroundColor, 1.2) :
               (control.hovered ? Qt.lighter(control.backgroundColor, 1.1) : control.backgroundColor)

        border.width: control.backgroundColor === "transparent" ? 1 : 0
        border.color: Style.border

        Behavior on color { ColorAnimation { duration: 100 } }
    }

    function rootTextColor() {
        if (backgroundColor === "transparent") return Style.textPrimary;
        return textColor;
    }
}
