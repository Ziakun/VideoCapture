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

    function syncValues() {
        xSpin.value = cropX
        ySpin.value = cropY
        wSpin.value = cropWidth
        hSpin.value = cropHeight
    }

    onCropXChanged: syncValues()
    onCropYChanged: syncValues()
    onCropWidthChanged: syncValues()
    onCropHeightChanged: syncValues()
    Component.onCompleted: syncValues()

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
                    id: cropPreview
                    x: Math.max(0, xSpin.value * root.mapScale)
                    y: Math.max(0, ySpin.value * root.mapScale)
                    width: Math.max(2, wSpin.value * root.mapScale)
                    height: Math.max(2, hSpin.value * root.mapScale)
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

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 10

            Label { text: "cropX"; color: "#c6ccd5"; font.pixelSize: 12; font.bold: true }
            NumericSpinBox {
                id: xSpin
                Layout.fillWidth: true
                from: 0
                to: Math.max(0, root.sourceWidth)
                accentColor: "#34d399"
            }

            Label { text: "cropY"; color: "#c6ccd5"; font.pixelSize: 12; font.bold: true }
            NumericSpinBox {
                id: ySpin
                Layout.fillWidth: true
                from: 0
                to: Math.max(0, root.sourceHeight)
                accentColor: "#34d399"
            }

            Label { text: "width"; color: "#c6ccd5"; font.pixelSize: 12; font.bold: true }
            NumericSpinBox {
                id: wSpin
                Layout.fillWidth: true
                from: 64
                to: Math.max(64, root.sourceWidth)
                accentColor: "#2dd4bf"
            }

            Label { text: "height"; color: "#c6ccd5"; font.pixelSize: 12; font.bold: true }
            NumericSpinBox {
                id: hSpin
                Layout.fillWidth: true
                from: 64
                to: Math.max(64, root.sourceHeight)
                accentColor: "#2dd4bf"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ControlButton {
                Layout.fillWidth: true
                text: "Apply"
                accentColor: "#34d399"
                baseColor: "#153226"
                onClicked: root.applyCrop(xSpin.value, ySpin.value, wSpin.value, hSpin.value)
            }

            ControlButton {
                Layout.fillWidth: true
                text: "Reset"
                accentColor: "#38bdf8"
                baseColor: "#142838"
                onClicked: root.resetCrop()
            }
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
