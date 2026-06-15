import QtQuick
import "../theme"

Rectangle {
    id: root

    radius: Theme.radiusLarge
    color: Theme.card
    border.color: Theme.border
    border.width: 1
    clip: true

    property alias title: titleText.text
    property alias subtitle: subtitleText.text
    default property alias content: container.data

    // Shadow effect (simulated for QML performance without GraphicalEffects)
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        z: -1
        radius: parent.radius
        color: "transparent"
        border.color: Qt.rgba(0, 0, 0, 0.2)
        border.width: 2
    }

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingMedium

        Column {
            width: parent.width
            spacing: Theme.spacingTiny
            visible: titleText.text !== ""

            Text {
                id: titleText
                width: parent.width
                font.family: Theme.fontFamily
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Text {
                id: subtitleText
                width: parent.width
                visible: text !== ""
                font.family: Theme.fontFamily
                font.pixelSize: 13
                color: Theme.textSecondary
            }
        }

        Item {
            id: container
            width: parent.width
            height: parent.height - (titleText.parent.visible ? titleText.parent.height + parent.spacing : 0)
        }
    }

    Behavior on color { ColorAnimation { duration: 200 } }
}
