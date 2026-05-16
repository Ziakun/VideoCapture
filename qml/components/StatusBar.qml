import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string selectedWindowTitle: ""
    property var selectedWindowId: 0
    property string selectedSourceType: "Window"
    property string captureModeText: ""
    property int cropX: 0
    property int cropY: 0
    property int cropWidth: 0
    property int cropHeight: 0
    property int sourceWidth: 0
    property int sourceHeight: 0
    property real fps: 0
    property bool isRecording: false
    property string outputFilePath: ""
    property string warningMessage: ""
    property string statusMessage: ""
    property string dismissedStatusText: ""

    signal dismissMessageRequested()

    function resetDismissedStatusTextIfNew() {
        if (statusMessageBox.rawText !== dismissedStatusText) {
            dismissedStatusText = ""
        }
    }

    onOutputFilePathChanged: resetDismissedStatusTextIfNew()
    onWarningMessageChanged: resetDismissedStatusTextIfNew()
    onStatusMessageChanged: resetDismissedStatusTextIfNew()

    height: 60
    color: "#0f151d"
    border.color: "#263344"

    function xidText() {
        if (Number(selectedWindowId) === 0) {
            return "-"
        }
        return "0x" + Number(selectedWindowId).toString(16)
    }

    function rightCrop() {
        return Math.max(0, root.sourceWidth - root.cropX - root.cropWidth)
    }

    function bottomCrop() {
        return Math.max(0, root.sourceHeight - root.cropY - root.cropHeight)
    }

    function cropText() {
        return "L " + root.cropX + " R " + rightCrop() + " T " + root.cropY + " B " + bottomCrop()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 8

        StatusChip {
            Layout.preferredWidth: 220
            label: "SOURCE"
            value: root.selectedWindowTitle.length > 0 ? root.selectedWindowTitle : "No source"
            accentColor: "#2dd4bf"
            active: root.selectedWindowTitle.length > 0
        }

        StatusChip {
            Layout.preferredWidth: 92
            label: "APP"
            value: Number(root.selectedWindowId) !== 0 ? root.selectedSourceType : "-"
            accentColor: root.selectedSourceType === "Zoom" ? "#60a5fa" : (root.selectedSourceType === "Browser" ? "#34d399" : "#94a3b8")
            valueColor: "#e5eef7"
            active: Number(root.selectedWindowId) !== 0
        }

        StatusChip {
            Layout.preferredWidth: 112
            label: "XID"
            value: root.xidText()
            accentColor: "#38bdf8"
            valueColor: "#bae6fd"
            monospaced: true
            active: Number(root.selectedWindowId) !== 0
        }

        StatusChip {
            Layout.preferredWidth: 160
            label: "MODE"
            value: root.captureModeText
            accentColor: root.captureModeText === "ScreenRegionFallback" ? "#f59e0b" : "#38bdf8"
            valueColor: "#dbeafe"
            active: root.captureModeText.length > 0
        }

        StatusChip {
            Layout.preferredWidth: 160
            label: "CROP"
            value: root.cropText()
            accentColor: "#34d399"
            valueColor: "#d1fae5"
            monospaced: true
            active: root.cropWidth > 0 && root.cropHeight > 0
            toolTipText: "CROP: left " + root.cropX + ", right " + root.rightCrop()
                + ", top " + root.cropY + ", bottom " + root.bottomCrop()
        }

        StatusChip {
            Layout.preferredWidth: 94
            label: "FPS"
            value: root.fps.toFixed(1)
            accentColor: root.fps > 0 ? "#2dd4bf" : "#475569"
            valueColor: "#ccfbf1"
            monospaced: true
            active: root.fps > 0
        }

        StatusChip {
            Layout.preferredWidth: 118
            label: "REC"
            value: root.isRecording ? "Recording" : "Idle"
            accentColor: root.isRecording ? "#ef4444" : "#22c55e"
            valueColor: root.isRecording ? "#fee2e2" : "#dcfce7"
            active: root.isRecording
            pulse: root.isRecording
        }

        Rectangle {
            id: statusMessageBox

            readonly property string rawText: root.warningMessage.length > 0
                ? root.warningMessage
                : (root.outputFilePath.length > 0 ? root.outputFilePath : root.statusMessage)
            readonly property string fullText: rawText === root.dismissedStatusText ? "" : rawText
            readonly property bool canDismiss: fullText.length > 0

            Layout.fillWidth: true
            Layout.preferredHeight: 34
            radius: 8
            color: root.warningMessage.length > 0 ? "#2a1f10" : "#121a24"
            border.color: root.warningMessage.length > 0 ? "#f59e0b" : "#293545"

            Label {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: statusMessageBox.canDismiss ? 38 : 12
                verticalAlignment: Text.AlignVCenter
                text: statusMessageBox.fullText
                color: root.warningMessage.length > 0 ? "#fde68a" : "#aab3c0"
                elide: Text.ElideMiddle
                font.pixelSize: 12
            }

            MouseArea {
                id: statusHoverArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                onContainsMouseChanged: {
                    if (containsMouse && statusMessageBox.fullText.length > 0) {
                        statusToolTipHideTimer.restart()
                        statusToolTip.open()
                    } else {
                        statusToolTipHideTimer.stop()
                        statusToolTip.close()
                    }
                }
            }

            Timer {
                id: statusToolTipHideTimer
                interval: 10000
                repeat: false
                onTriggered: statusToolTip.close()
            }

            ToolTip {
                id: statusToolTip
                text: statusMessageBox.fullText
                y: -implicitHeight - 8
                padding: 8
                contentItem: Text {
                    text: statusToolTip.text
                    color: "#f8fafc"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    width: Math.min(620, implicitWidth)
                }
                background: Rectangle {
                    radius: 6
                    color: "#111827"
                    border.color: "#334155"
                    border.width: 1
                }
            }

            Rectangle {
                id: statusCloseButton
                width: 24
                height: 24
                radius: 12
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 6
                color: closeStatusMouseArea.containsMouse ? "#243044" : "#172031"
                border.color: root.warningMessage.length > 0 ? "#f59e0b" : "#334155"
                visible: statusMessageBox.canDismiss && statusMessageBox.fullText.length > 0

                Label {
                    anchors.centerIn: parent
                    text: "x"
                    color: root.warningMessage.length > 0 ? "#fde68a" : "#cbd5e1"
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    id: closeStatusMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.dismissedStatusText = statusMessageBox.rawText
                        root.dismissMessageRequested()
                    }
                }
            }

            Behavior on border.color {
                ColorAnimation { duration: 160 }
            }
        }
    }
}
