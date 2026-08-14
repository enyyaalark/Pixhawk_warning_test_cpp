import QtQuick 2.12
import QtQuick.Window 2.12

Window {
    id: window
    visible: true
    width: 1280
    height: 820
    minimumWidth: 980
    minimumHeight: 680
    title: "Pixhawk PFD — Replay Prototype"
    color: "#070a0f"

    property color panel: "#101720"
    property color panelRaised: "#151f2a"
    property color line: "#344653"
    property color primaryText: "#f2f7fa"
    property color secondaryText: "#91a3b0"
    property color cyan: "#5bd5ff"
    property color amber: "#f0aa45"

    // Presentation smoothing only. Warning logic must use raw TelemetryData.
    property real displayRoll: pfdModel.rollDegrees
    property real displayPitch: pfdModel.pitchDegrees
    property real displayYaw: pfdModel.yawDegrees
    property real displayAltitude: pfdModel.relativeAltitudeMetres
    property real displayGroundSpeed: pfdModel.groundSpeedMetresPerSecond
    property real displayVerticalSpeed: pfdModel.verticalSpeedMetresPerSecond
    Behavior on displayRoll { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
    Behavior on displayPitch { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
    Behavior on displayYaw { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
    Behavior on displayAltitude { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
    Behavior on displayGroundSpeed { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
    Behavior on displayVerticalSpeed { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 66
        color: window.panel
        border.color: "#253540"

        Rectangle { width: parent.width; height: 2; color: window.cyan; opacity: 0.65 }

        Column {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: "PIXHAWK PFD"
                color: window.primaryText
                font.pixelSize: 20
                font.bold: true
                font.letterSpacing: 1.4
            }
            Text {
                text: "FLIGHT DATA VISUALIZATION"
                color: window.secondaryText
                font.pixelSize: 10
                font.letterSpacing: 1.8
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: sourceText.width + 32
            height: 34
            radius: 5
            color: "#172632"
            border.color: "#3b6378"
            Text {
                id: sourceText
                anchors.centerIn: parent
                text: pfdModel.sourceLabel
                color: window.cyan
                font.pixelSize: 13
                font.bold: true
                font.letterSpacing: 0.7
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 22
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14
            Rectangle {
                width: 112; height: 34; radius: 5
                color: pfdModel.connected ? "#153725" : "#30262a"
                border.color: pfdModel.connected ? "#43d47c" : window.amber
                Text {
                    anchors.centerIn: parent
                    text: pfdModel.connected ? "LINK VALID" : "LINK N/A"
                    color: pfdModel.connected ? "#74eaa0" : "#ffc268"
                    font.pixelSize: 12; font.bold: true
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: pfdModel.positionLabel
                color: window.secondaryText
                font.pixelSize: 12
            }
        }
    }

    Item {
        id: instrumentArea
        anchors.top: header.bottom
        anchors.bottom: cards.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16

        FlightTape {
            id: airspeedPanel
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.max(150, parent.width * 0.135)
            title: "GROUND SPEED"
            unit: "m/s · NOT IAS"
            value: window.displayGroundSpeed
            valid: pfdModel.groundSpeedValid
            majorStep: 2
            labelsOnLeft: false
            accent: window.cyan
        }

        AttitudeIndicator {
            id: attitude
            anchors.left: airspeedPanel.right
            anchors.right: rightColumn.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            rollDegrees: window.displayRoll
            pitchDegrees: window.displayPitch
            yawDegrees: window.displayYaw
            valid: pfdModel.attitudeValid
        }

        Column {
            id: rightColumn
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.max(205, parent.width * 0.19)
            spacing: 12

            FlightTape {
                id: altitudeTape
                width: parent.width
                height: parent.height * 0.54
                title: "REL ALTITUDE"
                unit: "METRES"
                value: window.displayAltitude
                valid: pfdModel.altitudeValid
                majorStep: 1
                labelsOnLeft: true
                accent: "#f3f7fa"
            }

            HeadingCompass {
                width: parent.width
                height: parent.height - altitudeTape.height - verticalSpeedText.height - parent.spacing * 2
                yawDegrees: window.displayYaw
                valid: pfdModel.attitudeValid
            }

            Text {
                id: verticalSpeedText
                anchors.horizontalCenter: parent.horizontalCenter
                text: pfdModel.verticalSpeedValid
                      ? "V/S  " + (window.displayVerticalSpeed >= 0 ? "+" : "") + window.displayVerticalSpeed.toFixed(2) + " m/s"
                      : "V/S  NO DATA"
                color: pfdModel.verticalSpeedValid ? window.primaryText : window.amber
                font.pixelSize: 10
                font.bold: true
            }
        }
    }

    Rectangle {
        id: cards
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 118
        color: "#0c1219"
        border.color: "#24343f"

        Row {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            StatusCard { width: (parent.width - 48) / 5; height: parent.height; title: "ROLL"; value: pfdModel.attitudeValid ? window.displayRoll.toFixed(1) + "°" : "—"; statusText: pfdModel.attitudeDataState; stateKind: pfdModel.attitudeValid ? "normal" : "unavailable" }
            StatusCard { width: (parent.width - 48) / 5; height: parent.height; title: "PITCH"; value: pfdModel.attitudeValid ? window.displayPitch.toFixed(1) + "°" : "—"; statusText: pfdModel.attitudeDataState; stateKind: pfdModel.attitudeValid ? "normal" : "unavailable" }
            StatusCard { width: (parent.width - 48) / 5; height: parent.height; title: "ATTITUDE YAW"; value: pfdModel.attitudeValid ? window.displayYaw.toFixed(1) + "°" : "—"; statusText: "NOT MAG HDG"; stateKind: pfdModel.attitudeValid ? "normal" : "unavailable" }
            StatusCard { width: (parent.width - 48) / 5; height: parent.height; title: "REL ALT"; value: pfdModel.altitudeValid ? window.displayAltitude.toFixed(1) + " m" : "—"; statusText: pfdModel.altitudeDataState; stateKind: pfdModel.altitudeValid ? "normal" : "unavailable" }
            StatusCard { width: (parent.width - 48) / 5; height: parent.height; title: "ALERT SYSTEM"; value: pfdModel.alertText; statusText: pfdModel.activeAlertCount > 0 ? pfdModel.activeAlertCount + " ACTIVE" : "BASE RULES ONLY"; stateKind: pfdModel.alertSeverity }
        }
    }
}
