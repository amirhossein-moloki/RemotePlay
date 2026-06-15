import QtQuick
import "../theme"

Rectangle {
    id: root

    property bool collapsed: false
    property int currentIndex: 0

    width: collapsed ? 68 : 250
    height: parent.height
    color: Theme.panel

    Rectangle {
        anchors.right: parent.right
        width: 1; height: parent.height
        color: Theme.border
    }

    Behavior on width {
        NumberAnimation { duration: 300; easing.type: Easing.OutQuint }
    }

    Column {
        anchors.fill: parent
        anchors.topMargin: Theme.spacingHuge
        spacing: Theme.spacingTiny

        // Logo Section
        Item {
            width: parent.width
            height: 64

            Row {
                anchors.left: parent.left
                anchors.leftMargin: root.collapsed ? (parent.width - 32) / 2 : Theme.spacingLarge
                spacing: Theme.spacingMedium

                Rectangle {
                    width: 32
                    height: 32
                    radius: 8
                    color: Theme.primary

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

                    opacity: root.collapsed ? 0 : 1
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }
            }
        }

        Item { width: 1; height: Theme.spacingLarge }

        SidebarItem {
            label: "Dashboard"
            icon: "" // Home icon
            active: root.currentIndex === 0
            collapsed: root.collapsed
            onClicked: root.currentIndex = 0
        }

        SidebarItem {
            label: "Settings"
            icon: "" // Settings icon
            active: root.currentIndex === 1
            collapsed: root.collapsed
            onClicked: root.currentIndex = 1
        }

        SidebarItem {
            label: "About"
            icon: "" // Info icon
            active: root.currentIndex === 2
            collapsed: root.collapsed
            onClicked: root.currentIndex = 2
        }

        Item { height: parent.height - y - 80; width: 1 } // Spacer

        SidebarItem {
            label: root.collapsed ? "Expand" : "Collapse"
            icon: root.collapsed ? "" : ""
            collapsed: root.collapsed
            onClicked: root.collapsed = !root.collapsed
        }
    }
}
