import QtQuick
import QtQuick.Controls

SpinBox {
    id: root

    property color accentColor: "#2dd4bf"

    implicitHeight: 38
    editable: true

    contentItem: TextInput {
        z: 2
        text: root.displayText
        color: root.enabled ? "#f8fafc" : "#7c8796"
        selectionColor: "#0f766e"
        selectedTextColor: "#ffffff"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        readOnly: !root.editable
        validator: root.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
        leftPadding: 8
        rightPadding: 32
        font.pixelSize: 13
        font.bold: true
    }

    up.indicator: Rectangle {
        x: root.width - width - 5
        y: 5
        width: 22
        height: Math.max(12, (root.height - 10) / 2)
        radius: 4
        color: root.up.pressed ? root.accentColor : (root.hovered ? "#293747" : "#202b38")

        Text {
            anchors.centerIn: parent
            text: "+"
            color: "#e5eef7"
            font.pixelSize: 12
            font.bold: true
        }
    }

    down.indicator: Rectangle {
        x: root.width - width - 5
        y: root.height - height - 5
        width: 22
        height: Math.max(12, (root.height - 10) / 2)
        radius: 4
        color: root.down.pressed ? root.accentColor : (root.hovered ? "#293747" : "#202b38")

        Text {
            anchors.centerIn: parent
            text: "−"
            color: "#e5eef7"
            font.pixelSize: 12
            font.bold: true
        }
    }

    background: Rectangle {
        radius: 8
        color: root.enabled ? "#141c26" : "#252b33"
        border.color: root.activeFocus ? root.accentColor : (root.hovered ? "#46566b" : "#303b4a")
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            radius: parent.radius
            color: "#35ffffff"
        }

        Behavior on border.color {
            ColorAnimation { duration: 120 }
        }
    }
}
