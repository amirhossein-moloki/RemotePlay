import QtQuick
import QtQuick.Layouts
import NexusDash

Rectangle {
    id: root

    property bool collapsed: false
    property int currentIndex: 0

    width: collapsed ? 72 : 250
    height: parent.height
    color: Theme.panel
    border.color: Theme.border
    border.width: 1

    Behavior on width {
        NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.spacingLarge
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
                    width: 40
                    height: 40
                    radius: 10
                    color: Theme.primary

                    Text {
                        anchors.centerIn: parent
                        text: "N"
                        color: "white"
                        font.bold: true
                        font.pixelSize: 22
                    }

                    // Glow effect
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: Theme.primary
                        opacity: 0.3
                        z: -1
                        scale: 1.1
                    }
                }

                Column {
                    visible: !root.collapsed
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "NexusDash"
                        font.family: Theme.fontFamily
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: "PREMIUM"
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        font.weight: Font.Black
                        color: Theme.accent
                        font.letterSpacing: 1
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Theme.spacingHuge }

        // Navigation Items
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

        Item { Layout.fillHeight: true }

        // Collapse Button
        SidebarItem {
            label: root.collapsed ? "Expand" : "Collapse"
            icon: root.collapsed ? "→" : "←"
            collapsed: root.collapsed
            onClicked: root.collapsed = !root.collapsed
        }
    }
}
