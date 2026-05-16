import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string selectedWindowTitle: ""
    property var selectedWindowId: 0
    property string captureModeText: ""
    property int cropX: 0
    property int cropY: 0
    property int cropWidth: 0
    property int cropHeight: 0
    property real fps: 0
    property bool isRecording: false
    property string outputFilePath: ""
    property string warningMessage: ""
    property string statusMessage: ""

    height: 60
    color: "#0f151d"
    border.color: "#263344"

    function xidText() {
        if (Number(selectedWindowId) === 0) {
            return "-"
        }
        return "0x" + Number(selectedWindowId).toString(16)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 8

        StatusChip {
            Layout.preferredWidth: 250
            label: "SOURCE"
            value: root.selectedWindowTitle.length > 0 ? root.selectedWindowTitle : "No source"
            accentColor: "#2dd4bf"
            active: root.selectedWindowTitle.length > 0
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
            value: root.cropX + "," + root.cropY + " " + root.cropWidth + "x" + root.cropHeight
            accentColor: "#34d399"
            valueColor: "#d1fae5"
            monospaced: true
            active: root.cropWidth > 0 && root.cropHeight > 0
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
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            radius: 8
            color: root.warningMessage.length > 0 ? "#2a1f10" : "#121a24"
            border.color: root.warningMessage.length > 0 ? "#f59e0b" : "#293545"

            Label {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                verticalAlignment: Text.AlignVCenter
                text: root.warningMessage.length > 0
                    ? root.warningMessage
                    : (root.outputFilePath.length > 0 ? root.outputFilePath : root.statusMessage)
                color: root.warningMessage.length > 0 ? "#fde68a" : "#aab3c0"
                elide: Text.ElideMiddle
                font.pixelSize: 12
            }

            Behavior on border.color {
                ColorAnimation { duration: 160 }
            }
        }
    }
}
