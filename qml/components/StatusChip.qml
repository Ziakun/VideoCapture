import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property color accentColor: "#2dd4bf"
    property color valueColor: "#e5eef7"
    property bool monospaced: false
    property bool active: false
    property bool pulse: false

    implicitHeight: 34
    radius: 8
    color: "#121a24"
    border.color: active ? accentColor : "#293545"
    border.width: 1
    clip: true

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 7

        Rectangle {
            id: dot
            Layout.preferredWidth: 7
            Layout.preferredHeight: 7
            radius: 4
            color: root.accentColor
            opacity: root.active ? 1 : 0.35

            SequentialAnimation on opacity {
                running: root.pulse
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.25; duration: 680; easing.type: Easing.InOutQuad }
                NumberAnimation { from: 0.25; to: 1.0; duration: 680; easing.type: Easing.InOutQuad }
            }
        }

        Label {
            text: root.label
            color: "#7f8da0"
            font.pixelSize: 10
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: root.value
            color: root.valueColor
            elide: Text.ElideMiddle
            font.pixelSize: 12
            font.bold: true
            font.family: root.monospaced ? "monospace" : ""
        }
    }

    Behavior on border.color {
        ColorAnimation { duration: 160 }
    }
}
