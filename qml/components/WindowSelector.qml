import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property var windows: []
    property var selectedXid: 0

    signal refreshRequested()
    signal selectWindow(var xid)

    function syncSelection() {
        for (var i = 0; i < windows.length; ++i) {
            if (Number(windows[i].xid) === Number(selectedXid)) {
                combo.currentIndex = i
                return
            }
        }
        combo.currentIndex = -1
    }

    onWindowsChanged: syncSelection()
    onSelectedXidChanged: syncSelection()

    ControlButton {
        Layout.preferredWidth: 138
        text: "Refresh Windows"
        accentColor: "#2dd4bf"
        baseColor: "#182532"
        onClicked: root.refreshRequested()
    }

    ComboBox {
        id: combo

        Layout.fillWidth: true
        Layout.preferredHeight: 40
        model: root.windows
        textRole: "display"
        valueRole: "xid"
        currentIndex: -1
        displayText: currentIndex >= 0 && root.windows[currentIndex]
            ? root.windows[currentIndex].title + "  " + root.windows[currentIndex].xidText
            : "Select window"

        contentItem: Text {
            leftPadding: 14
            rightPadding: 34
            text: combo.displayText
            color: combo.enabled ? "#f8fafc" : "#7c8796"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.pixelSize: 13
            font.bold: combo.currentIndex >= 0
        }

        indicator: Text {
            x: combo.width - width - 13
            y: combo.topPadding + (combo.availableHeight - height) / 2
            text: "⌄"
            color: "#94a3b8"
            font.pixelSize: 18
        }

        background: Rectangle {
            radius: 8
            color: combo.hovered ? "#202b38" : "#17212d"
            border.color: combo.pressed ? "#2dd4bf" : "#344256"
            border.width: 1

            Behavior on color {
                ColorAnimation { duration: 120 }
            }
            Behavior on border.color {
                ColorAnimation { duration: 120 }
            }
        }

        onActivated: function(index) {
            if (index >= 0 && root.windows[index]) {
                root.selectWindow(root.windows[index].xid)
            }
        }

        delegate: ItemDelegate {
            width: combo.width
            height: 54
            readonly property var itemData: typeof modelData !== "undefined" ? modelData : model

            background: Rectangle {
                color: hovered ? "#223044" : "#111827"
                border.color: hovered ? "#2dd4bf" : "#00000000"
            }

            contentItem: Item {
                Column {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 2

                    Text {
                        width: parent.width
                        text: itemData.title || ""
                        color: "#eef2f7"
                        elide: Text.ElideRight
                        font.pixelSize: 13
                    }

                    Text {
                        width: parent.width
                        text: (itemData.className || "-") + " | " + (itemData.xidText || "-") + " | " + (itemData.geometry || "-") + " | pid " + (itemData.pid || "-")
                        color: "#aab3c0"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
