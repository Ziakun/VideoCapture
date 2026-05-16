import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root

    property bool recording: false
    property bool captureRunning: false

    Layout.preferredWidth: 52
    Layout.preferredHeight: 42
    enabled: captureRunning || recording
    text: recording ? "●" : "▶"
    scale: down ? 0.96 : 1.0
    font.pixelSize: recording ? 21 : 19
    font.bold: true

    Behavior on scale {
        NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
    }

    contentItem: Text {
        text: root.text
        color: "white"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font: root.font
    }

    background: Item {
        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            radius: 10
            color: !root.enabled ? "#343941" : root.recording ? "#dc2626" : "#16a34a"
            border.color: root.hovered ? "#ffffff" : (root.recording ? "#fecaca" : "#bbf7d0")
            border.width: root.enabled ? 1 : 0

            Behavior on color {
                ColorAnimation { duration: 140 }
            }
            Behavior on border.color {
                ColorAnimation { duration: 140 }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 6
            height: parent.height + 6
            radius: 13
            color: "#00ffffff"
            border.color: root.recording ? "#ef4444" : "#22c55e"
            border.width: 1
            opacity: root.recording ? 0.4 : 0

            SequentialAnimation on opacity {
                running: root.recording
                loops: Animation.Infinite
                NumberAnimation { from: 0.55; to: 0.05; duration: 780; easing.type: Easing.OutCubic }
                NumberAnimation { from: 0.05; to: 0.55; duration: 0 }
            }
        }
    }

    ToolTip.visible: hovered
    ToolTip.text: recording ? "Stop recording" : "Start recording"
}
