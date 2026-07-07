import QtQuick
import QtQuick.Layouts
import "../theme"

Rectangle {
    id: root
    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    height: 40
    Layout.fillWidth: true
    radius: Theme.radiusMedium
    color: Theme.panel
    border.color: Theme.border
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: root.model
            delegate: Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: root.currentIndex === index ? Theme.surface : "transparent"
                    border.color: root.currentIndex === index ? Theme.border : "transparent"
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 200 } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: root.currentIndex === index ? Theme.textPrimary : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        font.weight: root.currentIndex === index ? Font.Bold : Font.Normal
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.currentIndex = index
                        root.activated(index)
                    }
                }
            }
        }
    }
}
