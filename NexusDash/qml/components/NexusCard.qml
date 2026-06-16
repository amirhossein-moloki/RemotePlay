import QtQuick
import NexusDash

Rectangle {
    id: root

    radius: Theme.radiusLarge
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    property alias title: titleText.text
    default property alias content: container.data

    // Premium Shadow
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 4
        z: -1
        radius: parent.radius
        color: "#000000"
        opacity: 0.2
    }

    // Glass-like Highlight
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "white"
        opacity: 0.02
        z: 1
        visible: true
        anchors.margins: 1
    }

    Column {
        anchors.fill: parent
        anchors.margins: Theme.adaptiveSpacing
        spacing: Theme.adaptiveSpacing

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
