import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

Item {
    id: root

    property alias model: listView.model
    property var columns: [] // List of { name: "Title", role: "roleName", width: 100 }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: Theme.panel

            Row {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingMedium

                Repeater {
                    model: root.columns
                    Text {
                        width: modelData.width
                        text: modelData.name
                        font.family: Theme.fontFamily
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: Theme.textSecondary
                        verticalAlignment: Text.AlignVCenter
                        height: parent.height
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }

        // Body
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                width: listView.width
                height: 56

                Rectangle {
                    anchors.fill: parent
                    color: itemMouseArea.containsMouse ? Qt.rgba(1,1,1,0.03) : "transparent"
                    Behavior on color { ColorAnimation { duration: 100 } }
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingLarge
                    anchors.rightMargin: Theme.spacingLarge
                    spacing: Theme.spacingMedium

                    Repeater {
                        model: root.columns
                        Item {
                            width: modelData.width
                            height: parent.height

                            Text {
                                anchors.fill: parent
                                text: model[modelData.role] || ""
                                font.family: Theme.fontFamily
                                font.pixelSize: 14
                                color: {
                                    if (modelData.role === "level") {
                                        if (text === "ERROR") return Theme.danger;
                                        if (text === "WARN") return Theme.warning;
                                        if (text === "INFO") return Theme.success;
                                    }
                                    return Theme.textPrimary;
                                }
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                MouseArea {
                    id: itemMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.border
                    opacity: 0.3
                }
            }
        }
    }
}
