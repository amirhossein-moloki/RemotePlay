import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "theme"
import "components"
import "pages"

ApplicationWindow {
    id: window
    width: 1280
    height: 850
    visible: true
    title: qsTr("NexusDash")
    color: Theme.background

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar
            Layout.fillHeight: true
            currentIndex: stackLayout.currentIndex
            onCurrentIndexChanged: stackLayout.currentIndex = currentIndex
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Top Header
            Rectangle {
                Layout.fillWidth: true
                height: 80
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingLarge
                    anchors.rightMargin: Theme.spacingLarge

                    Column {
                        Layout.fillWidth: true
                        Text {
                            text: stackLayout.currentIndex === 0 ? "Dashboard" :
                                  stackLayout.currentIndex === 1 ? "Settings" : "About"
                            font.family: Theme.fontFamily
                            font.pixelSize: 28
                            font.weight: Font.Bold
                            color: Theme.textPrimary
                        }
                        Text {
                            text: "Monitor and manage your remote streaming environment"
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            color: Theme.textSecondary
                        }
                    }

                    Row {
                        spacing: Theme.spacingLarge
                        Layout.alignment: Qt.AlignVCenter

                        // Connection Status Indicator
                        Row {
                            spacing: Theme.spacingSmall
                            anchors.verticalCenter: parent.verticalCenter

                            Rectangle {
                                width: 8; height: 8; radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: {
                                    let state = backend.system.connectionState;
                                    if (state === "Hosting") return Theme.accent;
                                    if (state === "Connected") return Theme.success;
                                    return Theme.danger;
                                }

                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.4; duration: 800 }
                                    NumberAnimation { to: 1.0; duration: 800 }
                                }
                            }

                            Text {
                                text: backend.system.connectionState
                                font.family: Theme.fontFamily
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                            }
                        }

                        // Date Time
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                text: Qt.formatDateTime(new Date(), "MMM dd, yyyy")
                                font.family: Theme.fontFamily
                                font.pixelSize: 12
                                color: Theme.textSecondary
                                horizontalAlignment: Text.AlignRight
                            }
                            Text {
                                text: Qt.formatDateTime(new Date(), "hh:mm AP")
                                font.family: Theme.fontFamily
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.border
                    opacity: 0.5
                }
            }

            // Main Content Area
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                StackLayout {
                    id: stackLayout
                    anchors.fill: parent
                    currentIndex: 0

                    DashboardPage {}
                    SettingsPage {}
                    AboutPage {}
                }
            }

            // Bottom Status Bar
            Rectangle {
                Layout.fillWidth: true
                height: 32
                color: Theme.panel

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: Theme.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMedium
                    anchors.rightMargin: Theme.spacingMedium

                    Text {
                        text: "NexusDash v1.0.0"
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }

                    Item { Layout.fillWidth: true }

                    Row {
                        spacing: Theme.spacingLarge
                        visible: backend.system.isSessionActive

                        Text {
                            text: "FPS: " + backend.system.fps.toFixed(0)
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }
                        Text {
                            text: "Bitrate: " + backend.system.bitrate.toFixed(1) + " Mbps"
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }
                        Text {
                            text: "Latency: " + backend.system.e2eLatency.toFixed(1) + " ms"
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }
                    }
                }
            }
        }
    }

    // Global transitions
    Behavior on color { ColorAnimation { duration: 300 } }
}
