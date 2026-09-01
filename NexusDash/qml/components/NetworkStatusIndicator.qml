import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme"

Item {
    id: root

    property real latency: 0
    property real loss: 0
    property bool showLabel: true

    readonly property color statusColor: {
        if (latency > 100 || loss > 0.05) return Theme.statusPoor
        if (latency > 40 || loss > 0.01) return Theme.statusFair
        return Theme.statusGood
    }

    readonly property string statusText: {
        if (latency > 100 || loss > 0.05) return qsTr("Poor")
        if (latency > 40 || loss > 0.01) return qsTr("Fair")
        return qsTr("Good")
    }

    width: layout.width
    height: layout.height

    Row {
        id: layout
        spacing: Theme.spacingSmall

        Rectangle {
            width: 10
            height: 10
            radius: 5
            color: root.statusColor
            anchors.verticalCenter: parent.verticalCenter

            // Pulse animation for poor connection
            SequentialAnimation on opacity {
                running: root.statusColor === Theme.statusPoor
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.4; duration: 500 }
                NumberAnimation { from: 0.4; to: 1.0; duration: 500 }
            }
        }

        Text {
            visible: root.showLabel
            text: root.statusText
            color: root.statusColor
            font.pixelSize: 11
            font.weight: Font.Bold
            font.family: Theme.fontFamily
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    ToolTip.visible: mouseArea.containsMouse
    ToolTip.text: qsTr("Latency: %1 ms\nLoss: %2%").arg(latency.toFixed(1)).arg((loss * 100).toFixed(2))

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
    }
}
