import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "theme"
import "components"
import "pages"

ApplicationWindow {
    id: window
    width: 1200
    height: 800
    visible: true
    title: qsTr("NexusDash")
    color: Theme.background

    Sidebar {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        currentIndex: stackLayout.currentIndex
        onCurrentIndexChanged: stackLayout.currentIndex = currentIndex
    }

    Item {
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        StackLayout {
            id: stackLayout
            anchors.fill: parent
            currentIndex: 0

            DashboardPage {}
            SettingsPage {}
            AboutPage {}
        }
    }

    NexusDialog {
        id: welcomeDialog
        title: "Welcome"
        message: "Welcome to NexusDash! This is a modern Qt 6 Dashboard template. Feel free to explore the different pages."
        Component.onCompleted: open()
    }

    // Global transitions
    Behavior on color { ColorAnimation { duration: 300 } }
}
