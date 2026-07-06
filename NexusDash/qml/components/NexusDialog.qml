import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

Dialog {
    id: control

    property alias message: messageText.text
    property string suggestion: ""

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 400
    modal: true
    focus: true

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLarge
        border.color: Theme.border
        border.width: 1
    }

    header: Rectangle {
        width: parent.width
        height: 50
        color: "transparent"
        Text {
            text: control.title
            anchors.centerIn: parent
            font.family: Theme.fontFamily
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingMedium
        Text {
            id: messageText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.family: Theme.fontFamily
            font.pixelSize: 14
            color: Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
        }

        // Action Suggestions
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: suggestionText.implicitHeight + 24
            color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.1)
            radius: Theme.radiusMedium
            visible: control.suggestion !== ""

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                Text { text: "💡"; font.pixelSize: 16 }
                Text {
                    id: suggestionText
                    text: control.suggestion
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    font.italic: true
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }

    footer: DialogButtonBox {
        background: Rectangle { color: "transparent" }
        alignment: Qt.AlignHCenter
        NexusButton {
            text: "Close"
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            primary: true
        }
    }
}
