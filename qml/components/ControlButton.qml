import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root

    property color accentColor: "#2dd4bf"
    property color baseColor: "#202833"
    property color disabledColor: "#303640"
    property bool compact: false
    property int textPixelSize: compact ? 12 : 13
    property string toolTipText: text

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
        font.pixelSize: root.textPixelSize
        font.bold: true
    }

    background: Rectangle {
        radius: 8
        color: root.enabled
            ? (root.down ? Qt.darker(root.accentColor, 1.25) : (root.hovered ? Qt.lighter(root.baseColor, 1.18) : root.baseColor))
            : root.disabledColor
        border.color: root.enabled ? (root.hovered ? root.accentColor : "#354152") : "#303640"
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
        Behavior on border.color {
            ColorAnimation { duration: 120 }
        }
    }

    onHoveredChanged: {
        if (hovered && toolTipText.length > 0) {
            toolTipHideTimer.restart()
            buttonToolTip.open()
        } else {
            toolTipHideTimer.stop()
            buttonToolTip.close()
        }
    }

    onToolTipTextChanged: {
        if (buttonToolTip.visible && toolTipText.length === 0) {
            buttonToolTip.close()
        }
    }

    Timer {
        id: toolTipHideTimer
        interval: 10000
        repeat: false
        onTriggered: buttonToolTip.close()
    }

    ToolTip {
        id: buttonToolTip
        text: root.toolTipText
        y: root.height + 8
        padding: 8
        contentItem: Text {
            text: buttonToolTip.text
            color: "#f8fafc"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            width: Math.min(520, implicitWidth)
        }
        background: Rectangle {
            radius: 6
            color: "#111827"
            border.color: "#334155"
            border.width: 1
        }
    }
}
