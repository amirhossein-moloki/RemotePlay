import QtQuick
import "../theme"

Item {
    id: root
    property var dataModel: []
    property color lineWeightColor: Theme.primary
    property real maxValue: 100
    property bool fillEnabled: true

    onDataModelChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            if (!root.dataModel || root.dataModel.length < 2) return;

            var step = width / (root.dataModel.length - 1);

            // Draw Fill
            if (root.fillEnabled) {
                ctx.beginPath();
                ctx.moveTo(0, height);
                for (var i = 0; i < root.dataModel.length; i++) {
                    var val = root.dataModel[i];
                    var y = height - (val / root.maxValue * height);
                    ctx.lineTo(i * step, y);
                }
                ctx.lineTo(width, height);
                ctx.closePath();

                var gradient = ctx.createLinearGradient(0, 0, 0, height);
                gradient.addColorStop(0, Qt.rgba(root.lineWeightColor.r, root.lineWeightColor.g, root.lineWeightColor.b, 0.3));
                gradient.addColorStop(1, Qt.rgba(root.lineWeightColor.r, root.lineWeightColor.g, root.lineWeightColor.b, 0.0));
                ctx.fillStyle = gradient;
                ctx.fill();
            }

            // Draw Line
            ctx.beginPath();
            ctx.lineWidth = 2;
            ctx.strokeStyle = root.lineWeightColor;
            ctx.lineJoin = "round";

            for (var j = 0; j < root.dataModel.length; j++) {
                var val2 = root.dataModel[j];
                var y2 = height - (val2 / root.maxValue * height);
                if (j === 0) ctx.moveTo(0, y2);
                else ctx.lineTo(j * step, y2);
            }
            ctx.stroke();
        }
    }
}
