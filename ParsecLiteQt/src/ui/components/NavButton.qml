import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ParsecLite.UI

Button {
    id: control
    property string iconText: ""
    property bool isActive: false

    Layout.fillWidth: true
    contentItem: RowLayout {
        spacing: Style.spacingMedium
        Text {
            text: control.iconText
            font.pixelSize: 18
        }
        Text {
            text: control.text
            font: Style.bodyFont
            font.bold: control.isActive
            color: control.isActive ? Style.accent : Style.textPrimary
            Layout.fillWidth: true
        }
    }

    background: Rectangle {
        implicitHeight: 40
        radius: Style.radius
        color: control.isActive ?
               (appBridge.darkMode ? Qt.rgba(0, 122, 255, 0.15) : Qt.rgba(0, 122, 255, 0.1)) :
               (control.hovered ? (appBridge.darkMode ? "#2A2A2A" : "#E8E8E8") : "transparent")

        Behavior on color { ColorAnimation { duration: 150 } }
    }
}
