import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ParsecLite.UI

// Import subdirectories for components and pages
import "components"
import "pages"

ApplicationWindow {
    id: window
    width: 1000
    height: 700
    visible: true
    title: "Parsec-Lite Desktop"
    color: Style.background

    property string currentPage: "Dashboard"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar
        Rectangle {
            id: sidebar
            Layout.fillHeight: true
            Layout.preferredWidth: 240
            color: Style.surface

            Rectangle {
                anchors.right: parent.right
                height: parent.height
                width: 1
                color: Style.border
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Style.spacingMedium
                spacing: Style.spacingSmall

                // Logo/App Name
                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Style.spacingLarge
                    spacing: Style.spacingMedium

                    Rectangle {
                        width: 32
                        height: 32
                        radius: 6
                        color: Style.accent
                        Text {
                            anchors.centerIn: parent
                            text: "P"
                            color: "white"
                            font.bold: true
                            font.pixelSize: 18
                        }
                    }

                    Text {
                        text: "Parsec-Lite"
                        font: Style.headerFont
                        font.pixelSize: 20
                        color: Style.textPrimary
                    }
                }

                // Nav Items
                NavButton {
                    text: "Dashboard"
                    iconText: "🏠"
                    isActive: window.currentPage === "Dashboard"
                    onClicked: window.currentPage = "Dashboard"
                }

                NavButton {
                    text: "Settings"
                    iconText: "⚙️"
                    isActive: window.currentPage === "Settings"
                    onClicked: window.currentPage = "Settings"
                }

                NavButton {
                    text: "About"
                    iconText: "ℹ️"
                    isActive: window.currentPage === "About"
                    onClicked: window.currentPage = "About"
                }

                Spacer { Layout.fillHeight: true }

                // User Profile / Theme Toggle
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Style.border
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Style.spacingSmall
                    spacing: Style.spacingMedium

                    Rectangle {
                        width: 36
                        height: 36
                        radius: 18
                        color: Style.border
                        Text {
                            anchors.centerIn: parent
                            text: appBridge.userName.substring(0, 1)
                            color: Style.textPrimary
                        }
                    }

                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: appBridge.userName
                            font: Style.bodyFont
                            font.bold: true
                            color: Style.textPrimary
                        }
                        Text {
                            text: "Online"
                            font: Style.smallFont
                            color: "#4CAF50"
                        }
                    }
                }

                CustomButton {
                    Layout.fillWidth: true
                    text: appBridge.darkMode ? "Light Mode" : "Dark Mode"
                    onClicked: appBridge.toggleTheme()
                    backgroundColor: "transparent"
                    border.color: Style.border
                }
            }
        }

        // Main Content Area
        StackLayout {
            id: contentStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: {
                if (window.currentPage === "Dashboard") return 0
                if (window.currentPage === "Settings") return 1
                if (window.currentPage === "About") return 2
                return 0
            }

            Dashboard {}
            Settings {}
            About {}
        }
    }

    // Helper Spacer component defined inline
    component Spacer : Item {
        Layout.fillWidth: true
        Layout.fillHeight: false
    }
}
