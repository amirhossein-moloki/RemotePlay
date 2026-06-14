import QtQuick
import QtQuick.Layouts
import ParsecLite.UI

Rectangle {
    id: root
    property string title: ""
    property alias content: container.data

    Layout.fillWidth: true
    implicitHeight: mainCol.implicitHeight + (Style.spacingLarge * 2)

    color: Style.surface
    radius: Style.radius
    border.color: Style.border
    border.width: 1

    ColumnLayout {
        id: mainCol
        anchors.fill: parent
        anchors.margins: Style.spacingLarge
        spacing: Style.spacingMedium

        Text {
            text: root.title
            font: Style.headerFont
            font.pixelSize: 18
            color: Style.textPrimary
            visible: text !== ""
        }

        Item {
            id: container
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
