import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeetVideoCapture 1.0

Item {
    id: root

    property var frameProvider
    property string message: ""
    property string warning: ""
    property bool recording: false
    property bool captureRunning: false
    property string recordingTimeText: "00:00:00"
    property real fps: 0
    property string outputFilePath: ""

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#091018" }
            GradientStop { position: 1.0; color: "#05070a" }
        }
    }

    VideoPreviewItem {
        id: preview
        anchors.fill: parent
        anchors.margins: 18
        frameProvider: root.frameProvider
    }

    Rectangle {
        id: previewFrame
        anchors.fill: preview
        color: "#00000000"
        radius: 14
        border.width: 1
        border.color: root.recording ? "#ef4444" : (root.captureRunning ? "#34d399" : "#334155")
        opacity: 0.9

        Behavior on border.color {
            ColorAnimation { duration: 180 }
        }
    }

    Rectangle {
        id: scanLine
        width: Math.max(120, preview.width * 0.22)
        height: 2
        radius: 1
        y: preview.y
        x: preview.x - width
        color: root.recording ? "#ef4444" : "#2dd4bf"
        opacity: root.captureRunning ? 0.65 : 0

        SequentialAnimation on x {
            running: root.captureRunning
            loops: Animation.Infinite
            NumberAnimation {
                from: preview.x - scanLine.width
                to: preview.x + preview.width
                duration: 1800
                easing.type: Easing.InOutCubic
            }
            PauseAnimation { duration: 350 }
        }

        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }
    }

    RecordingTimerPill {
        id: floatingTimer
        recording: root.recording
        timeText: root.recordingTimeText
        filePath: root.outputFilePath
        width: implicitWidth
        height: implicitHeight
        x: root.recording ? preview.x + preview.width - width - 16 : preview.x + preview.width + 18
        y: preview.y + 16

        Behavior on x {
            NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: fpsBadge
        width: 94
        height: 32
        radius: 16
        anchors.left: preview.left
        anchors.top: preview.top
        anchors.leftMargin: 16
        anchors.topMargin: 16
        color: "#cc111827"
        border.color: root.captureRunning ? "#2dd4bf" : "#334155"
        opacity: root.captureRunning ? 1 : 0

        Label {
            anchors.centerIn: parent
            text: root.fps.toFixed(1) + " FPS"
            color: "#d1fae5"
            font.pixelSize: 12
            font.bold: true
        }

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }
        Behavior on border.color {
            ColorAnimation { duration: 160 }
        }
    }

    Rectangle {
        id: messagePanel
        anchors.centerIn: parent
        width: Math.min(parent.width - 96, 560)
        height: Math.max(96, messageLabel.implicitHeight + 36)
        radius: 14
        color: "#dd0f172a"
        border.color: root.message.indexOf("black or stale") >= 0 ? "#f59e0b" : "#475569"
        visible: opacity > 0
        opacity: root.message.length > 0 ? 1 : 0
        scale: root.message.length > 0 ? 1 : 0.96

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }
        Behavior on scale {
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }

        Label {
            id: messageLabel
            anchors.centerIn: parent
            width: parent.width - 36
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.message
            color: root.message.indexOf("black or stale") >= 0 ? "#ffd166" : "#d8dde6"
            font.pixelSize: 22
            font.bold: true
        }
    }

    Rectangle {
        id: warningDrawer
        height: Math.max(42, warningText.implicitHeight + 18)
        radius: 10
        x: preview.x + 16
        y: root.warning.length > 0 ? preview.y + preview.height - height - 16 : preview.y + preview.height + 18
        width: Math.max(0, preview.width - 32)
        color: "#dd2a1f10"
        border.color: "#f59e0b"
        visible: root.warning.length > 0 || y < preview.y + preview.height

        Label {
            id: warningText
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: "#fde68a"
            text: root.warning
            font.pixelSize: 12
        }

        Behavior on y {
            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
        }
    }
}
