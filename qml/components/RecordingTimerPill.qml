import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property bool recording: false
    property string timeText: "00:00:00"
    property string filePath: ""

    implicitWidth: recording ? 176 : 0
    implicitHeight: 40
    visible: width > 1 || opacity > 0
    opacity: recording ? 1 : 0
    clip: true

    Behavior on implicitWidth {
        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
    }
    Behavior on opacity {
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: 20
        color: "#24161a"
        border.color: "#ef4444"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 9

            Item {
                Layout.preferredWidth: 12
                Layout.preferredHeight: 12

                Rectangle {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    radius: 5
                    color: "#ef4444"
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    radius: 5
                    color: "#00ffffff"
                    border.color: "#ef4444"
                    border.width: 1
                    opacity: root.recording ? 0.7 : 0
                    scale: root.recording ? 2.0 : 1.0

                    SequentialAnimation on scale {
                        running: root.recording
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 2.0; duration: 900; easing.type: Easing.OutCubic }
                        NumberAnimation { from: 2.0; to: 1.0; duration: 0 }
                    }

                    SequentialAnimation on opacity {
                        running: root.recording
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.65; to: 0.0; duration: 900; easing.type: Easing.OutCubic }
                        NumberAnimation { from: 0.0; to: 0.65; duration: 0 }
                    }
                }
            }

            Label {
                text: root.timeText
                color: "#fee2e2"
                font.family: "monospace"
                font.pixelSize: 15
                font.bold: true
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }

            Label {
                Layout.fillWidth: true
                text: "REC"
                color: "#fca5a5"
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    ToolTip.visible: recording && mouseArea.containsMouse && filePath.length > 0
    ToolTip.text: filePath

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }
}
