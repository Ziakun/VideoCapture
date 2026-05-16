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

    signal applyCrop(int x, int y, int width, int height)
    signal resetCrop()

    color: "#0f151e"
    border.color: "#263344"

    readonly property real mapScale: sourceWidth > 0 && sourceHeight > 0
        ? Math.min((cropMap.width - 24) / sourceWidth, (cropMap.height - 24) / sourceHeight)
        : 0
    readonly property real mapWidth: Math.max(1, sourceWidth * mapScale)
    readonly property real mapHeight: Math.max(1, sourceHeight * mapScale)
    readonly property int minimumCropSize: 64
    property bool syncing: false

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

        Item {
            id: cropMap
            Layout.fillWidth: true
            Layout.preferredHeight: 154

            Rectangle {
                anchors.fill: parent
                radius: 12
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#111c28" }
                    GradientStop { position: 1.0; color: "#0b111a" }
                }
                border.color: "#27364a"
            }

            Rectangle {
                id: sourceMap
                width: root.mapWidth
                height: root.mapHeight
                anchors.centerIn: parent
                radius: 7
                color: "#101820"
                border.color: "#475569"

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: 6
                    color: "#0a0f15"
                    opacity: 0.88
                }

                Rectangle {
                    id: leftCutPreview
                    x: 0
                    y: 0
                    width: root.effectiveCuts().left * root.mapScale
                    height: parent.height
                    color: "#101820cc"
                    visible: width > 0
                }

                Rectangle {
                    id: rightCutPreview
                    x: parent.width - width
                    y: 0
                    width: root.effectiveCuts().right * root.mapScale
                    height: parent.height
                    color: "#101820cc"
                    visible: width > 0
                }

                Rectangle {
                    id: topCutPreview
                    x: 0
                    y: 0
                    width: parent.width
                    height: root.effectiveCuts().top * root.mapScale
                    color: "#111827bb"
                    visible: height > 0
                }

                Rectangle {
                    id: bottomCutPreview
                    x: 0
                    y: parent.height - height
                    width: parent.width
                    height: root.effectiveCuts().bottom * root.mapScale
                    color: "#111827bb"
                    visible: height > 0
                }

                Rectangle {
                    id: cropPreview
                    x: root.effectiveCuts().left * root.mapScale
                    y: root.effectiveCuts().top * root.mapScale
                    width: Math.max(2, (root.sourceWidth - root.effectiveCuts().left - root.effectiveCuts().right) * root.mapScale)
                    height: Math.max(2, (root.sourceHeight - root.effectiveCuts().top - root.effectiveCuts().bottom) * root.mapScale)
                    radius: 5
                    color: "#223c2e55"
                    border.color: "#34d399"
                    border.width: 2

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 2
                        radius: 1
                        color: "#a7f3d0"
                    }

                    Behavior on x { NumberAnimation { duration: 120 } }
                    Behavior on y { NumberAnimation { duration: 120 } }
                    Behavior on width { NumberAnimation { duration: 120 } }
                    Behavior on height { NumberAnimation { duration: 120 } }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "left"; color: "#c6ccd5"; font.pixelSize: 12; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        text: root.effectiveCuts().left
                        color: "#f8fafc"
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Slider {
                    id: leftSlider
                    Layout.fillWidth: true
                    from: 0
                    to: root.horizontalLimit()
                    stepSize: 1
                    live: true
                    onMoved: {
                        root.clampActiveSlider("left")
                        root.scheduleApply()
                    }
                    onPressedChanged: if (!pressed) {
                        root.clampActiveSlider("left")
                        root.applySliderCrop()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "right"; color: "#c6ccd5"; font.pixelSize: 12; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        text: root.effectiveCuts().right
                        color: "#f8fafc"
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Slider {
                    id: rightSlider
                    Layout.fillWidth: true
                    from: 0
                    to: root.horizontalLimit()
                    stepSize: 1
                    live: true
                    onMoved: {
                        root.clampActiveSlider("right")
                        root.scheduleApply()
                    }
                    onPressedChanged: if (!pressed) {
                        root.clampActiveSlider("right")
                        root.applySliderCrop()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "top"; color: "#c6ccd5"; font.pixelSize: 12; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        text: root.effectiveCuts().top
                        color: "#f8fafc"
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Slider {
                    id: topSlider
                    Layout.fillWidth: true
                    from: 0
                    to: root.verticalLimit()
                    stepSize: 1
                    live: true
                    onMoved: {
                        root.clampActiveSlider("top")
                        root.scheduleApply()
                    }
                    onPressedChanged: if (!pressed) {
                        root.clampActiveSlider("top")
                        root.applySliderCrop()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "bottom"; color: "#c6ccd5"; font.pixelSize: 12; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        text: root.effectiveCuts().bottom
                        color: "#f8fafc"
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Slider {
                    id: bottomSlider
                    Layout.fillWidth: true
                    from: 0
                    to: root.verticalLimit()
                    stepSize: 1
                    live: true
                    onMoved: {
                        root.clampActiveSlider("bottom")
                        root.scheduleApply()
                    }
                    onPressedChanged: if (!pressed) {
                        root.clampActiveSlider("bottom")
                        root.applySliderCrop()
                    }
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
