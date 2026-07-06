import QtQuick
import QtQuick.Layouts
import "../theme"

Rectangle {
    id: root
    property string message: ""
    property bool isError: true

    width: Math.min(400, parent.width - 40)
    height: 60
    radius: Theme.radiusMedium
    color: Theme.surface
    border.color: isError ? Theme.accent : Theme.success
    border.width: 1

    opacity: 0
    visible: opacity > 0

    anchors.top: parent.top
    anchors.topMargin: 20
    anchors.horizontalCenter: parent.horizontalCenter

    layer.enabled: true
    // drop shadow would go here if needed

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingMedium

        Rectangle {
            width: 32; height: 32; radius: 16
            color: isError ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1) : Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.1)
            Text {
                anchors.centerIn: parent
                text: isError ? "!" : "✓"
                color: isError ? Theme.accent : Theme.success
                font.bold: true
                font.pixelSize: 18
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.message
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignRight // Support Farsi
        }
    }

    function show(msg, error = true) {
        message = msg
        isError = error
        showAnim.restart()
    }

    SequentialAnimation {
        id: showAnim
        NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; duration: 300; easing.type: Easing.OutQuad }
        PauseAnimation { duration: 5000 }
        NumberAnimation { target: root; property: "opacity"; from: 1; to: 0; duration: 500; easing.type: Easing.InQuad }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.opacity = 0
    }
}
