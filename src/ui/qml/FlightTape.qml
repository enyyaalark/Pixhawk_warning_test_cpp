import QtQuick 2.12

Item {
    id: root
    property string title: "VALUE"
    property string unit: ""
    property real value: 0
    property bool valid: false
    property real majorStep: 10
    // true for the right-side altitude tape: labels sit at the outside/right,
    // tick marks and the current-value pointer face the attitude display/left.
    property bool labelsOnLeft: false
    property color accent: "#ffffff"

    Text {
        anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
        text: root.title; color: "#8da4b0"; font.pixelSize: 10
        font.bold: true; font.letterSpacing: 0.9
    }

    Item {
        id: scale
        anchors.top: parent.top; anchors.topMargin: 24
        anchors.bottom: parent.bottom; anchors.bottomMargin: 22
        anchors.left: parent.left; anchors.right: parent.right
        clip: false

        // Continuous vertical datum line makes the scale direction explicit.
        Rectangle {
            anchors.top: parent.top; anchors.bottom: parent.bottom
            x: root.labelsOnLeft ? 25 : parent.width - 27
            width: 1; color: "#758b97"; opacity: root.valid ? 0.9 : 0.35
        }

        Repeater {
            model: 19
            delegate: Item {
                property int offset: index - 9
                property bool major: offset % 2 === 0
                property real halfStep: root.majorStep / 2
                property real tickValue: Math.round(root.value / halfStep) * halfStep + offset * halfStep
                width: scale.width; height: 18
                y: scale.height / 2 - height / 2
                   - (tickValue - root.value) * scale.height / (root.majorStep * 6)
                visible: root.valid && y > -height && y < scale.height

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    x: root.labelsOnLeft ? 25 : parent.width - 27 - width
                    width: major ? 24 : 12; height: major ? 2 : 1
                    color: major ? "#f1f7fa" : "#8fa3ae"
                }
                Text {
                    visible: major
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: root.labelsOnLeft ? parent.left : undefined
                    anchors.right: root.labelsOnLeft ? undefined : parent.right
                    anchors.leftMargin: root.labelsOnLeft ? 58 : 0
                    anchors.rightMargin: root.labelsOnLeft ? 0 : 58
                    text: tickValue.toFixed(root.majorStep < 1 ? 1 : 0)
                    color: "#e7f0f4"; font.pixelSize: 12; font.bold: true
                }
            }
        }

        // Current-value pointer: rectangular readout plus a nose extending
        // toward the attitude display, as on a conventional PFD tape.
        Canvas {
            id: pointer
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width + 13; height: 44
            x: root.labelsOnLeft ? -13 : 0
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.fillStyle = "#ed060b10"
                ctx.strokeStyle = root.valid ? root.accent : "#6f7e87"
                ctx.lineWidth = 2
                ctx.beginPath()
                if (root.labelsOnLeft) {
                    ctx.moveTo(1, height / 2)
                    ctx.lineTo(14, 8); ctx.lineTo(width - 1, 8)
                    ctx.lineTo(width - 1, height - 8); ctx.lineTo(14, height - 8)
                } else {
                    ctx.moveTo(width - 1, height / 2)
                    ctx.lineTo(width - 14, 8); ctx.lineTo(1, 8)
                    ctx.lineTo(1, height - 8); ctx.lineTo(width - 14, height - 8)
                }
                ctx.closePath(); ctx.fill(); ctx.stroke()
            }
            Connections {
                target: root
                function onValidChanged() { pointer.requestPaint() }
                function onAccentChanged() { pointer.requestPaint() }
            }
            Text {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: root.labelsOnLeft ? 6 : -6
                text: root.valid ? root.value.toFixed(1) : "— —"
                color: root.valid ? "white" : "#768690"
                font.pixelSize: 19; font.bold: true
            }
        }
    }

    Text {
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
        text: root.valid ? root.unit : "NO DATA"
        color: root.valid ? "#8da4b0" : "#e6a744"
        font.pixelSize: 9; font.bold: true
    }
}
