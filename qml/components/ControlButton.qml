import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root

    property color accentColor: "#2dd4bf"
    property color baseColor: "#202833"
    property color disabledColor: "#303640"
    property bool compact: false

    Layout.preferredHeight: compact ? 36 : 40
    implicitHeight: compact ? 36 : 40
    leftPadding: compact ? 12 : 14
    rightPadding: compact ? 12 : 14

    contentItem: Text {
        text: root.text
        color: root.enabled ? "#f8fafc" : "#7c8796"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        font.pixelSize: root.compact ? 12 : 13
        font.bold: true
    }

    background: Rectangle {
        radius: 8
        color: root.enabled
            ? (root.down ? Qt.darker(root.accentColor, 1.25) : (root.hovered ? Qt.lighter(root.baseColor, 1.18) : root.baseColor))
            : root.disabledColor
        border.color: root.enabled ? (root.hovered ? root.accentColor : "#354152") : "#303640"
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            radius: parent.radius
            color: root.enabled ? "#55ffffff" : "#18ffffff"
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
        Behavior on border.color {
            ColorAnimation { duration: 120 }
        }
    }
}
