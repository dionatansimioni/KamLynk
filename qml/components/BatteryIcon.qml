import QtQuick
import KamLynk

// Battery outline whose fill follows `level` (0-100, -1 unknown).
Item {
    id: battery
    property int level: -1
    readonly property color fillColor: level < 0 ? Theme.textDim
                                     : level <= 15 ? Theme.record
                                     : level <= 30 ? Theme.warn : Theme.ok
    implicitWidth: 26
    implicitHeight: 13

    Rectangle {
        id: body
        width: parent.width - 3
        height: parent.height
        radius: 3
        color: "transparent"
        border.color: Theme.text
        border.width: 1.5
        Rectangle {
            x: 3; y: 3
            height: parent.height - 6
            width: battery.level < 0 ? 0 : Math.max(2, (parent.width - 6) * battery.level / 100)
            radius: 1.5
            color: battery.fillColor
        }
    }
    Rectangle {
        anchors.left: body.right
        anchors.leftMargin: 0.5
        anchors.verticalCenter: body.verticalCenter
        width: 2.5
        height: body.height * 0.42
        radius: 1
        color: Theme.text
    }
}
