import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string edgeName: ""
    property string label: ""
    property int valueText: 0
    property real limit: 0
    property alias value: slider.value
    readonly property bool pressed: slider.pressed

    signal edgeMoved(string edgeName)
    signal edgeReleased(string edgeName)

    Layout.fillWidth: true
    spacing: 4

    RowLayout {
        Layout.fillWidth: true

        Label {
            text: root.label
            color: "#c6ccd5"
            font.pixelSize: 12
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            text: root.valueText
            color: "#f8fafc"
            horizontalAlignment: Text.AlignRight
            font.pixelSize: 12
            font.bold: true
        }
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: 0
        to: root.limit
        stepSize: 1
        live: true
        onMoved: root.edgeMoved(root.edgeName)
        onPressedChanged: if (!pressed) {
            root.edgeReleased(root.edgeName)
        }
    }
}
