import QtQuick

Item {
    id: root

    property int sourceWidth: 0
    property int sourceHeight: 0
    property int leftCut: 0
    property int rightCut: 0
    property int topCut: 0
    property int bottomCut: 0

    readonly property real mapScale: sourceWidth > 0 && sourceHeight > 0
        ? Math.min((width - 24) / sourceWidth, (height - 24) / sourceHeight)
        : 0
    readonly property real mapWidth: Math.max(1, sourceWidth * mapScale)
    readonly property real mapHeight: Math.max(1, sourceHeight * mapScale)

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
            x: 0
            y: 0
            width: root.leftCut * root.mapScale
            height: parent.height
            color: "#101820cc"
            visible: width > 0
        }

        Rectangle {
            x: parent.width - width
            y: 0
            width: root.rightCut * root.mapScale
            height: parent.height
            color: "#101820cc"
            visible: width > 0
        }

        Rectangle {
            x: 0
            y: 0
            width: parent.width
            height: root.topCut * root.mapScale
            color: "#111827bb"
            visible: height > 0
        }

        Rectangle {
            x: 0
            y: parent.height - height
            width: parent.width
            height: root.bottomCut * root.mapScale
            color: "#111827bb"
            visible: height > 0
        }

        Rectangle {
            x: root.leftCut * root.mapScale
            y: root.topCut * root.mapScale
            width: Math.max(2, (root.sourceWidth - root.leftCut - root.rightCut) * root.mapScale)
            height: Math.max(2, (root.sourceHeight - root.topCut - root.bottomCut) * root.mapScale)
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
