import QtQuick
import "../theme"

Rectangle {
    id: root

    property bool collapsed: false
    property int currentIndex: 0

    width: collapsed ? 64 : 240
    height: parent.height
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    Behavior on width {
        NumberAnimation { duration: 250; easing.type: Easing.InOutQuad }
    }

    Column {
        anchors.fill: parent
        anchors.topMargin: Theme.spacingLarge
        spacing: Theme.spacingTiny

        // Logo Section
        Item {
            width: parent.width
            height: 60

            Row {
                anchors.centerIn: parent
                spacing: Theme.spacingSmall

                Rectangle {
                    width: 32
                    height: 32
                    radius: 8
                    color: Theme.accent

                    Text {
                        anchors.centerIn: parent
                        text: "N"
                        color: "white"
                        font.bold: true
                        font.pixelSize: 18
                    }
                }

                Text {
                    text: "NexusDash"
                    visible: !root.collapsed
                    font.family: Theme.fontFamily
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Item { width: 1; height: Theme.spacingLarge }

        SidebarItem {
            label: "Dashboard"
            icon: "D"
            active: root.currentIndex === 0
            collapsed: root.collapsed
            onClicked: root.currentIndex = 0
        }

        SidebarItem {
            label: "Settings"
            icon: "S"
            active: root.currentIndex === 1
            collapsed: root.collapsed
            onClicked: root.currentIndex = 1
        }

        SidebarItem {
            label: "About"
            icon: "A"
            active: root.currentIndex === 2
            collapsed: root.collapsed
            onClicked: root.currentIndex = 2
        }

        Item { height: parent.height - y - 60; width: 1 } // Spacer

        SidebarItem {
            label: root.collapsed ? "Expand" : "Collapse"
            icon: root.collapsed ? ">" : "<"
            collapsed: root.collapsed
            onClicked: root.collapsed = !root.collapsed
        }
    }
}
