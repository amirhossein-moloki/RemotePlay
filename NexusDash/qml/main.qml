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
    minimumWidth: 1000
    minimumHeight: 700
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

    // Main Content Area
    ColumnLayout {
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0

        StackLayout {
            id: stackLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0

            DashboardPage {}
            SettingsPage {}
            AboutPage {}
        }

        // Bottom Status Bar
        Rectangle {
            Layout.fillWidth: true
            height: 32
            color: Theme.panel
            border.color: Theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMedium
                anchors.rightMargin: Theme.spacingMedium
                spacing: Theme.spacingLarge

                Text {
                    text: "NexusDash v1.0.0"
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    font.family: Theme.fontFamily
                }

                Item { Layout.fillWidth: true }

                Row {
                    spacing: Theme.spacingMedium
                    Layout.alignment: Qt.AlignVCenter

                    StatusIndicator {
                        label: "STATUS"
                        value: backend.system.isSessionActive ? "CONNECTED" : "IDLE"
                        valueColor: backend.system.isSessionActive ? Theme.success : Theme.textSecondary
                    }

                    Rectangle { width: 1; height: 16; color: Theme.border; Layout.alignment: Qt.AlignVCenter }

                    StatusIndicator {
                        label: "FPS"
                        value: backend.system.fps.toFixed(0)
                        visible: backend.system.isSessionActive
                    }

                    StatusIndicator {
                        label: "BITRATE"
                        value: backend.system.bitrate.toFixed(1) + " Mbps"
                        visible: backend.system.isSessionActive
                    }

                    StatusIndicator {
                        label: "LATENCY"
                        value: backend.system.e2eLatency.toFixed(1) + " ms"
                        visible: backend.system.isSessionActive
                    }
                }
            }
        }
    }

    // Helper component for status bar
    component StatusIndicator : Row {
        property string label: ""
        property string value: ""
        property color valueColor: Theme.textPrimary
        spacing: Theme.spacingSmall
        visible: true

        Text {
            text: label
            color: Theme.textSecondary
            font.pixelSize: 10
            font.weight: Font.Bold
            font.family: Theme.fontFamily
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: value
            color: valueColor
            font.pixelSize: 11
            font.weight: Font.Medium
            font.family: Theme.monoFontFamily
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // Global transitions
    Behavior on color { ColorAnimation { duration: 300 } }
}
