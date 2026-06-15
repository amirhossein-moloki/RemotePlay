import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import NexusDash

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
            height: 40
            color: Theme.surfaceSecondary
            radius: Theme.radiusSmall

            Row {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMedium
                anchors.rightMargin: Theme.spacingMedium
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
        }

        // Body
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            delegate: Item {
                width: listView.width
                height: 48

                Rectangle {
                    anchors.fill: parent
                    color: index % 2 === 0 ? "transparent" : Qt.rgba(Theme.surfaceSecondary.r, Theme.surfaceSecondary.g, Theme.surfaceSecondary.b, 0.3)
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMedium
                    anchors.rightMargin: Theme.spacingMedium
                    spacing: Theme.spacingMedium

                    Repeater {
                        model: root.columns
                        Text {
                            width: modelData.width
                            text: model[modelData.role] || ""
                            font.family: Theme.fontFamily
                            font.pixelSize: 14
                            color: Theme.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            height: parent.height
                            elide: Text.ElideRight
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
        }
    }
}
