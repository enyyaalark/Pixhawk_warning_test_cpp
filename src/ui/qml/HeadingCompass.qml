import QtQuick 2.12

Rectangle {
    id: root
    property real yawDegrees: 0
    property bool valid: false
    radius: 8
    color: "#101720"
    border.color: "#344653"

    Text { anchors.top: parent.top; anchors.topMargin: 12; anchors.horizontalCenter: parent.horizontalCenter; text: "ATTITUDE YAW"; color: "#91a3b0"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 1 }

    Item {
        id: compass
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 10
        width: Math.min(parent.width - 24, parent.height - 52)
        height: width
        visible: root.valid

        Rectangle { anchors.fill: parent; radius: width / 2; color: "#0a1016"; border.color: "#69808e"; border.width: 2 }

        Item {
            anchors.fill: parent
            rotation: -root.yawDegrees
            Repeater {
                model: 36
                delegate: Rectangle {
                    property int degrees: index * 10
                    x: compass.width / 2 - 1
                    y: degrees % 30 === 0 ? 7 : 10
                    width: degrees % 30 === 0 ? 3 : 2
                    height: degrees % 30 === 0 ? 14 : 8
                    color: "#eaf2f6"
                    transform: Rotation { origin.x: width / 2; origin.y: compass.height / 2 - y; angle: degrees }
                }
            }
            Repeater {
                model: [0, 90, 180, 270]
                delegate: Text {
                    property int degrees: modelData
                    property string cardinal: degrees === 0 ? "N" : degrees === 90 ? "E" : degrees === 180 ? "S" : "W"
                    x: compass.width / 2 - width / 2 + Math.sin(degrees * Math.PI / 180) * compass.width * 0.31
                    y: compass.height / 2 - height / 2 - Math.cos(degrees * Math.PI / 180) * compass.height * 0.31
                    text: cardinal
                    color: cardinal === "N" ? "#ff6a6a" : "#ecf4f7"
                    font.pixelSize: 15; font.bold: true
                    rotation: root.yawDegrees
                }
            }
        }

        Text { anchors.top: parent.top; anchors.topMargin: -5; anchors.horizontalCenter: parent.horizontalCenter; text: "▼"; color: "#ffd13b"; font.pixelSize: 20; font.bold: true }
        Rectangle { anchors.centerIn: parent; width: 70; height: 34; radius: 4; color: "#d0131d26"; border.color: "#547184"; Text { anchors.centerIn: parent; text: root.yawDegrees.toFixed(0) + "°"; color: "white"; font.pixelSize: 18; font.bold: true } }
    }

    Text { anchors.bottom: parent.bottom; anchors.bottomMargin: 8; anchors.horizontalCenter: parent.horizontalCenter; text: valid ? "NOT VALIDATED MAGNETIC HEADING" : "YAW DATA INVALID"; color: valid ? "#d9a552" : "#ef7474"; font.pixelSize: 9; font.bold: true }
}
