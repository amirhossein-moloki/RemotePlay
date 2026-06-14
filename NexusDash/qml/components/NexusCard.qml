import QtQuick
import "../theme"

Rectangle {
    id: root

    radius: Theme.radiusLarge
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    property alias title: titleText.text
    default property alias content: container.data

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingMedium

        Text {
            id: titleText
            visible: text !== ""
            font.family: Theme.fontFamily
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        Item {
            id: container
            width: parent.width
            height: parent.height - (titleText.visible ? titleText.height + parent.spacing : 0)
        }
    }
}
