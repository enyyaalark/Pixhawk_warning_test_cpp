import QtQuick 2.12

Rectangle {
    id: root
    property string title: ""
    property string value: "—"
    property string statusText: ""
    property string stateKind: "normal"
    property color accent: stateKind === "normal" ? "#50d790" : stateKind === "critical" ? "#ff3030" : stateKind === "warning" ? "#ff6060" : stateKind === "caution" ? "#f0aa45" : stateKind === "standby" ? "#5bd5ff" : "#f0aa45"

    radius: 7
    color: "#131c25"
    border.color: accent
    border.width: 1

    Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 4; radius: 2; color: root.accent }
    Text { anchors.left: parent.left; anchors.leftMargin: 16; anchors.top: parent.top; anchors.topMargin: 10; text: root.title; color: "#91a3b0"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }
    Text { anchors.left: parent.left; anchors.leftMargin: 16; anchors.verticalCenter: parent.verticalCenter; anchors.verticalCenterOffset: 4; text: root.value; color: "#f2f7fa"; font.pixelSize: 23; font.bold: true }
    Text { anchors.right: parent.right; anchors.rightMargin: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 9; text: root.statusText; color: root.accent; font.pixelSize: 9; font.bold: true }
}
