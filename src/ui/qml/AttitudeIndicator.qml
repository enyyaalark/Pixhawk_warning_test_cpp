import QtQuick 2.12

Item {
    id: root
    property real rollDegrees: 0
    property real pitchDegrees: 0
    property real yawDegrees: 0
    property bool valid: false
    property real apertureWidth: Math.min(width * 0.76, height * 1.02)
    property real apertureHeight: Math.min(height * 0.86, apertureWidth * 0.92)
    property real pixelsPerDegree: Math.max(3.7, apertureHeight / 90)

    // Lightweight Boeing-style attitude aperture: the dark surround is left
    // open for tapes and annunciations instead of becoming another panel.
    Item {
        id: aperture
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 14
        width: root.apertureWidth
        height: root.apertureHeight

        Canvas {
            id: pfdCanvas
            anchors.fill: parent
            antialiasing: true

            function roundedPath(ctx, x, y, w, h, r) {
                ctx.beginPath()
                ctx.moveTo(x + r, y)
                ctx.lineTo(x + w - r, y)
                ctx.quadraticCurveTo(x + w, y, x + w, y + r)
                ctx.lineTo(x + w, y + h - r)
                ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
                ctx.lineTo(x + r, y + h)
                ctx.quadraticCurveTo(x, y + h, x, y + h - r)
                ctx.lineTo(x, y + r)
                ctx.quadraticCurveTo(x, y, x + r, y)
                ctx.closePath()
            }

            onPaint: {
                var ctx = getContext("2d")
                var cx = width / 2
                var cy = height / 2
                var extent = Math.max(width, height) * 5
                var corner = Math.min(34, width * 0.07)
                ctx.reset()
                roundedPath(ctx, 0, 0, width, height, corner)
                ctx.clip()
                ctx.fillStyle = "#071019"
                ctx.fillRect(0, 0, width, height)
                if (!root.valid)
                    return

                ctx.save()
                ctx.translate(cx, cy + root.pitchDegrees * root.pixelsPerDegree)
                ctx.rotate(-root.rollDegrees * Math.PI / 180)
                ctx.fillStyle = "#1689bd"
                ctx.fillRect(-extent, -extent, extent * 2, extent)
                ctx.fillStyle = "#8a542f"
                ctx.fillRect(-extent, 0, extent * 2, extent)

                // Horizon and ladder move together as the earth reference.
                ctx.strokeStyle = "#ffffff"
                ctx.lineWidth = 3
                ctx.beginPath(); ctx.moveTo(-extent, 0); ctx.lineTo(extent, 0); ctx.stroke()

                var angles = [-30, -25, -20, -15, -10, -5, 5, 10, 15, 20, 25, 30]
                ctx.fillStyle = "#ffffff"
                ctx.strokeStyle = "#ffffff"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.font = "bold 12px sans-serif"
                for (var i = 0; i < angles.length; ++i) {
                    var angle = angles[i]
                    var major = angle % 10 === 0
                    var y = -angle * root.pixelsPerDegree
                    var markWidth = major ? width * 0.29 : width * 0.15
                    ctx.lineWidth = major ? 2.2 : 1.4
                    ctx.beginPath(); ctx.moveTo(-markWidth / 2, y); ctx.lineTo(markWidth / 2, y); ctx.stroke()
                    if (major) {
                        ctx.fillText(Math.abs(angle), -markWidth / 2 - 18, y)
                        ctx.fillText(Math.abs(angle), markWidth / 2 + 18, y)
                    }
                }
                ctx.restore()

                // Fixed bank scale, deliberately sparse like an airliner PFD.
                var radius = Math.min(width * 0.40, height * 0.46)
                var marks = [-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60]
                ctx.strokeStyle = "#f5fbff"
                for (var j = 0; j < marks.length; ++j) {
                    var bank = marks[j]
                    var rad = (bank - 90) * Math.PI / 180
                    var len = bank % 30 === 0 ? 13 : 7
                    ctx.lineWidth = bank % 30 === 0 ? 2.5 : 1.5
                    ctx.beginPath()
                    ctx.moveTo(cx + (radius - len) * Math.cos(rad), cy + (radius - len) * Math.sin(rad))
                    ctx.lineTo(cx + radius * Math.cos(rad), cy + radius * Math.sin(rad))
                    ctx.stroke()
                }
            }

            Connections {
                target: root
                function onRollDegreesChanged() { pfdCanvas.requestPaint() }
                function onPitchDegreesChanged() { pfdCanvas.requestPaint() }
                function onValidChanged() { pfdCanvas.requestPaint() }
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        // Thin fixed aircraft reference, kept visually separate from the ladder.
        Item {
            anchors.centerIn: parent
            width: parent.width * 0.42
            height: 38
            Rectangle { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.39; height: 3; color: "#f4d72e" }
            Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.39; height: 3; color: "#f4d72e" }
            Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 4; color: "#f4d72e" }
        }

        // Zero-bank sky pointer and slip/skid bar remain screen-fixed.
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 3
            width: 38; height: 28
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "▼"; color: "#f4d72e"; font.pixelSize: 20; font.bold: true }
            Rectangle { anchors.top: parent.top; anchors.topMargin: 20; anchors.horizontalCenter: parent.horizontalCenter; width: 22; height: 3; color: "#ffffff" }
        }

        Rectangle {
            anchors.fill: parent
            radius: Math.min(34, width * 0.07)
            visible: !root.valid
            color: "#ed071019"
            Text { anchors.centerIn: parent; text: "ATT"; color: "#ff3b3b"; font.pixelSize: 30; font.bold: true }
        }
    }

    // Screen-fixed heading reference. The scale moves horizontally beneath
    // the lubber line, providing yaw feedback in the pilot's primary scan.
    Item {
        id: headingTape
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 5
        width: root.apertureWidth
        height: 54
        clip: true

        Repeater {
            model: 25
            delegate: Item {
                property int relativeStep: index - 12
                property real rawHeading: Math.round(root.yawDegrees / 5) * 5 + relativeStep * 5
                property real normalizedHeading: ((rawHeading % 360) + 360) % 360
                property real delta: rawHeading - root.yawDegrees
                property bool major: Math.round(normalizedHeading) % 10 === 0
                width: 42; height: headingTape.height
                x: headingTape.width / 2 - width / 2 + delta * headingTape.width / 70

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: major ? 2 : 1; height: major ? 12 : 7
                    color: major ? "#f1f7fa" : "#8298a4"
                }
                Text {
                    visible: major
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 16
                    text: {
                        var h = Math.round(normalizedHeading)
                        if (h === 0) return "N"
                        if (h === 90) return "E"
                        if (h === 180) return "S"
                        if (h === 270) return "W"
                        return (h / 10).toFixed(0)
                    }
                    color: "#e9f2f6"; font.pixelSize: 11; font.bold: true
                }
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: headingValue.width + 18; height: 22; radius: 3
            color: "#e6081017"; border.color: "#55d5ff"
            Text {
                id: headingValue; anchors.centerIn: parent
                text: {
                    var heading = Math.round(((root.yawDegrees % 360) + 360) % 360)
                    return (heading < 10 ? "00" : heading < 100 ? "0" : "") + heading + "°"
                }
                color: "white"; font.pixelSize: 12; font.bold: true
            }
        }

        // Lubber line extends downward into the attitude field.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top; anchors.topMargin: 22
            width: 2; height: 32; color: "#55d5ff"
        }
    }

    // The fixed lateral reference deliberately crosses the aperture boundary,
    // visually tying the attitude display to the adjacent flight tapes.
    Rectangle { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; width: Math.max(34, (parent.width - root.apertureWidth) / 2 + 24); height: 2; color: "#eaf4f8"; opacity: 0.82 }
    Rectangle { anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right; width: Math.max(34, (parent.width - root.apertureWidth) / 2 + 24); height: 2; color: "#eaf4f8"; opacity: 0.82 }

    // Compact readout floats on the surround; no heavy enclosing card.
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        text: root.valid ? "ROLL " + root.rollDegrees.toFixed(1) + "°    PITCH " + root.pitchDegrees.toFixed(1) + "°" : "ATTITUDE INVALID"
        color: root.valid ? "#a9bec9" : "#ff6060"
        font.pixelSize: 11
        font.bold: true
        font.letterSpacing: 0.5
    }
}
