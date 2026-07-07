import QtQuick
import QtQuick.Layouts
import "../theme"

Rectangle {
    id: root

    property bool collapsed: false
    property int currentIndex: 0

    width: collapsed ? 72 : (Theme.isSmall ? 220 : 250)
    height: parent.height
    color: Theme.panel
    border.color: Theme.border
    border.width: 0

    // Modern right border shadow/line
    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    Behavior on width {
        NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.spacingHuge
        anchors.bottomMargin: Theme.spacingLarge
        spacing: Theme.spacingTiny

        // Logo Section
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.leftMargin: root.collapsed ? 0 : Theme.spacingMedium

            Row {
                anchors.centerIn: parent
                spacing: Theme.spacingMedium

                Rectangle {
                    width: 44
                    height: 44
                    radius: 12
                    color: Theme.primary

                    Text {
                        anchors.centerIn: parent
                        text: "N"
                        color: "white"
                        font.bold: true
                        font.pixelSize: 24
                    }

                    // Glow effect
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: Theme.primary
                        opacity: 0.2
                        z: -1
                        scale: 1.15
                    }
                }

                Column {
                    visible: !root.collapsed
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "NexusDash"
                        font.family: Theme.fontFamily
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: "ENTERPRISE"
                        font.family: Theme.fontFamily
                        font.pixelSize: 9
                        font.weight: Font.Black
                        color: Theme.accent
                        font.letterSpacing: 1.5
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Theme.spacingGiga }

        // Navigation Items
        SidebarItem {
            label: "Dashboard"
            icon: "󰕒"
            active: root.currentIndex === 0
            collapsed: root.collapsed
            onClicked: root.currentIndex = 0
        }

        SidebarItem {
            label: "Settings"
            icon: "󰒓"
            active: root.currentIndex === 1
            collapsed: root.collapsed
            onClicked: root.currentIndex = 1
        }

        SidebarItem {
            label: "About"
            icon: "󰋼"
            active: root.currentIndex === 2
            collapsed: root.collapsed
            onClicked: root.currentIndex = 2
        }

        Item { Layout.fillHeight: true }

        // Collapse Button
        SidebarItem {
            label: root.collapsed ? "Expand" : "Collapse"
            icon: root.collapsed ? "󰁔" : "󰁍"
            collapsed: root.collapsed
            onClicked: root.collapsed = !root.collapsed
        }
    }
}
