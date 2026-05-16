import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeetVideoCapture 1.0
import "components"

ApplicationWindow {
    id: root

    width: 1280
    height: 760
    minimumWidth: 980
    minimumHeight: 620
    visible: true
    title: "MeetVideoCapture"
    color: "#0b0f14"

    header: ToolBar {
        id: toolbar
        height: 72

        background: Rectangle {
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1b2430" }
                GradientStop { position: 1.0; color: "#111821" }
            }
            border.color: "#293544"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#34d399"
                opacity: captureController.isCapturing ? 0.65 : 0.18

                Behavior on opacity {
                    NumberAnimation { duration: 180 }
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            spacing: 12

            WindowSelector {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                windows: captureController.windows
                selectedXid: captureController.selectedWindowId
                onRefreshRequested: captureController.refreshWindows()
                onSelectWindow: function(xid) {
                    captureController.selectWindow(xid)
                }
            }

            ControlButton {
                Layout.preferredWidth: 116
                text: captureController.isCapturing ? "Stop Capture" : "Start Capture"
                accentColor: captureController.isCapturing ? "#f97316" : "#34d399"
                baseColor: captureController.isCapturing ? "#3a2419" : "#163228"
                onClicked: captureController.isCapturing ? captureController.stopCapture() : captureController.startCapture()
            }

            ControlButton {
                Layout.preferredWidth: 94
                text: "Fallback"
                accentColor: "#38bdf8"
                baseColor: "#172b3b"
                enabled: captureController.selectedWindowId !== 0
                onClicked: captureController.switchToScreenRegionFallback()
                ToolTip.visible: hovered
                ToolTip.text: "Use visible screen region capture"
            }

            RecordButton {
                recording: captureController.isRecording
                captureRunning: captureController.isCapturing
                onClicked: captureController.toggleRecording()
            }

            RecordingTimerPill {
                recording: captureController.isRecording
                timeText: captureController.recordingTimeText
                filePath: captureController.outputFilePath
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        VideoPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            frameProvider: captureController.frameProvider
            message: captureController.previewMessage
            warning: captureController.warningMessage
            recording: captureController.isRecording
            recordingTimeText: captureController.recordingTimeText
            captureRunning: captureController.isCapturing
            fps: captureController.fps
            outputFilePath: captureController.outputFilePath
        }

        CropControls {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            cropX: captureController.cropX
            cropY: captureController.cropY
            cropWidth: captureController.cropWidth
            cropHeight: captureController.cropHeight
            sourceWidth: captureController.sourceWidth
            sourceHeight: captureController.sourceHeight
            sourceGeometryText: captureController.sourceGeometryText
            captureModeText: captureController.captureModeText
            enabled: captureController.selectedWindowId !== 0
            onApplyCrop: function(x, y, w, h) {
                captureController.setCropRect(x, y, w, h)
            }
            onResetCrop: {
                captureController.setCropRect(0, 0, captureController.sourceWidth, captureController.sourceHeight)
            }
        }
    }

    footer: StatusBar {
        selectedWindowTitle: captureController.selectedWindowTitle
        selectedWindowId: captureController.selectedWindowId
        captureModeText: captureController.captureModeText
        cropX: captureController.cropX
        cropY: captureController.cropY
        cropWidth: captureController.cropWidth
        cropHeight: captureController.cropHeight
        fps: captureController.fps
        isRecording: captureController.isRecording
        outputFilePath: captureController.outputFilePath
        warningMessage: captureController.warningMessage
        statusMessage: captureController.statusMessage
    }
}
