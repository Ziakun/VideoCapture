import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property int cropX: 0
    property int cropY: 0
    property int cropWidth: 0
    property int cropHeight: 0
    property int sourceWidth: 0
    property int sourceHeight: 0
    property string sourceGeometryText: "-"
    property string captureModeText: "X11WindowById"
    property bool syncing: false

    readonly property int minimumCropSize: 64

    signal applyCrop(int x, int y, int width, int height)
    signal resetCrop()

    color: "#0f151e"
    border.color: "#263344"

    function syncValues() {
        syncing = true
        leftSlider.value = Math.max(0, cropX)
        rightSlider.value = Math.max(0, sourceWidth - cropX - cropWidth)
        topSlider.value = Math.max(0, cropY)
        bottomSlider.value = Math.max(0, sourceHeight - cropY - cropHeight)
        syncing = false
    }

    function rounded(value) {
        return Math.round(value)
    }

    function clamped(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value))
    }

    function horizontalLimit() {
        return Math.max(0, root.sourceWidth - root.minimumCropSize)
    }

    function verticalLimit() {
        return Math.max(0, root.sourceHeight - root.minimumCropSize)
    }

    function clampActiveSlider(edge) {
        if (syncing) {
            return
        }

        if (edge === "left") {
            leftSlider.value = root.clamped(root.rounded(leftSlider.value), 0, Math.max(0, root.horizontalLimit() - root.rounded(rightSlider.value)))
        } else if (edge === "right") {
            rightSlider.value = root.clamped(root.rounded(rightSlider.value), 0, Math.max(0, root.horizontalLimit() - root.rounded(leftSlider.value)))
        } else if (edge === "top") {
            topSlider.value = root.clamped(root.rounded(topSlider.value), 0, Math.max(0, root.verticalLimit() - root.rounded(bottomSlider.value)))
        } else if (edge === "bottom") {
            bottomSlider.value = root.clamped(root.rounded(bottomSlider.value), 0, Math.max(0, root.verticalLimit() - root.rounded(topSlider.value)))
        }
    }

    function effectiveCuts() {
        var horizontalLimit = root.horizontalLimit()
        var verticalLimit = root.verticalLimit()
        var left = root.clamped(root.rounded(leftSlider.value), 0, horizontalLimit)
        var right = root.clamped(root.rounded(rightSlider.value), 0, horizontalLimit)
        var top = root.clamped(root.rounded(topSlider.value), 0, verticalLimit)
        var bottom = root.clamped(root.rounded(bottomSlider.value), 0, verticalLimit)

        if (left + right > horizontalLimit) {
            if (leftSlider.pressed) {
                left = Math.max(0, horizontalLimit - right)
            } else if (rightSlider.pressed) {
                right = Math.max(0, horizontalLimit - left)
            } else {
                left = Math.max(0, horizontalLimit - right)
            }
        }
        if (top + bottom > verticalLimit) {
            if (topSlider.pressed) {
                top = Math.max(0, verticalLimit - bottom)
            } else if (bottomSlider.pressed) {
                bottom = Math.max(0, verticalLimit - top)
            } else {
                top = Math.max(0, verticalLimit - bottom)
            }
        }

        return { "left": left, "right": right, "top": top, "bottom": bottom }
    }

    function scheduleApply() {
        if (syncing || root.sourceWidth <= 0 || root.sourceHeight <= 0) {
            return
        }
        applyTimer.restart()
    }

    function applySliderCrop() {
        if (syncing || root.sourceWidth <= 0 || root.sourceHeight <= 0) {
            return
        }
        var cuts = root.effectiveCuts()
        root.applyCrop(
            cuts.left,
            cuts.top,
            root.sourceWidth - cuts.left - cuts.right,
            root.sourceHeight - cuts.top - cuts.bottom)
    }

    onCropXChanged: syncValues()
    onCropYChanged: syncValues()
    onCropWidthChanged: syncValues()
    onCropHeightChanged: syncValues()
    onSourceWidthChanged: syncValues()
    onSourceHeightChanged: syncValues()
    Component.onCompleted: syncValues()

    Timer {
        id: applyTimer
        interval: 80
        repeat: false
        onTriggered: root.applySliderCrop()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: "Crop"
                color: "#f8fafc"
                font.pixelSize: 20
                font.bold: true
            }

            Rectangle {
                Layout.preferredHeight: 26
                Layout.preferredWidth: 104
                radius: 13
                color: "#142635"
                border.color: "#38bdf8"

                Label {
                    anchors.centerIn: parent
                    text: root.captureModeText
                    color: "#bae6fd"
                    elide: Text.ElideRight
                    width: parent.width - 16
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Source " + root.sourceGeometryText
            color: "#aab3c0"
            elide: Text.ElideRight
        }

        CropMap {
            id: cropMap
            Layout.fillWidth: true
            Layout.preferredHeight: 154
            sourceWidth: root.sourceWidth
            sourceHeight: root.sourceHeight
            leftCut: root.effectiveCuts().left
            rightCut: root.effectiveCuts().right
            topCut: root.effectiveCuts().top
            bottomCut: root.effectiveCuts().bottom
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            CropEdgeSlider {
                id: leftSlider
                label: "left"
                edgeName: "left"
                valueText: root.effectiveCuts().left
                limit: root.horizontalLimit()
                onEdgeMoved: function(edge) {
                    root.clampActiveSlider(edge)
                    root.scheduleApply()
                }
                onEdgeReleased: function(edge) {
                    root.clampActiveSlider(edge)
                    root.applySliderCrop()
                }
            }

            CropEdgeSlider {
                id: rightSlider
                label: "right"
                edgeName: "right"
                valueText: root.effectiveCuts().right
                limit: root.horizontalLimit()
                onEdgeMoved: function(edge) {
                    root.clampActiveSlider(edge)
                    root.scheduleApply()
                }
                onEdgeReleased: function(edge) {
                    root.clampActiveSlider(edge)
                    root.applySliderCrop()
                }
            }

            CropEdgeSlider {
                id: topSlider
                label: "top"
                edgeName: "top"
                valueText: root.effectiveCuts().top
                limit: root.verticalLimit()
                onEdgeMoved: function(edge) {
                    root.clampActiveSlider(edge)
                    root.scheduleApply()
                }
                onEdgeReleased: function(edge) {
                    root.clampActiveSlider(edge)
                    root.applySliderCrop()
                }
            }

            CropEdgeSlider {
                id: bottomSlider
                label: "bottom"
                edgeName: "bottom"
                valueText: root.effectiveCuts().bottom
                limit: root.verticalLimit()
                onEdgeMoved: function(edge) {
                    root.clampActiveSlider(edge)
                    root.scheduleApply()
                }
                onEdgeReleased: function(edge) {
                    root.clampActiveSlider(edge)
                    root.applySliderCrop()
                }
            }
        }

        ControlButton {
            Layout.fillWidth: true
            text: "Reset"
            accentColor: "#38bdf8"
            baseColor: "#142838"
            onClicked: root.resetCrop()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#263344"
        }

        Label {
            Layout.fillWidth: true
            text: "min 64x64"
            color: "#687789"
            horizontalAlignment: Text.AlignRight
            font.pixelSize: 11
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
