import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ParsecLite.UI

// No components needed here currently, but keeping consistent
import "../components"

Item {
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 100, 500)
        spacing: Style.spacingLarge

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 80
            height: 80
            radius: 20
            color: Style.accent
            Text {
                anchors.centerIn: parent
                text: "P"
                color: "white"
                font.bold: true
                font.pixelSize: 48
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Parsec-Lite"
            font: Style.headerFont
            color: Style.textPrimary
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Version 1.0.0 (Build 2025)"
            font: Style.bodyFont
            color: Style.textSecondary
        }

        Text {
            Layout.fillWidth: true
            text: "Parsec-Lite is a high-performance LAN game streaming system designed for low latency and high visual fidelity. Built with C++17 and Qt 6."
            font: Style.bodyFont
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Style.textPrimary
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Style.border
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Style.spacingLarge

            Text {
                text: "GitHub"
                font: Style.bodyFont
                color: Style.accent
            }
            Text {
                text: "Documentation"
                font: Style.bodyFont
                color: Style.accent
            }
            Text {
                text: "License"
                font: Style.bodyFont
                color: Style.accent
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "© 2025 Parsec-Lite Contributors"
            font: Style.smallFont
            color: Style.textSecondary
        }
    }
}
