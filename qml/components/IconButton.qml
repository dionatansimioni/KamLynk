import QtQuick
import QtQuick.Controls
import KamLynk

// Flat rounded button showing a short glyph/text or an SVG icon. Checkable buttons
// (toggles) get the accent tint while on.
AbstractButton {
    id: control
    property color color: Theme.panelRaised
    property color textColor: Theme.text
    property int size: 40
    property string tip: ""
    property url iconSource
    property int iconSize: 20

    readonly property bool hasIcon: iconSource.toString().length > 0

    implicitWidth: hasIcon && text.length === 0 ? size : Math.max(size, contentItem.implicitWidth + 20)
    implicitHeight: size
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.35

    ToolTip.visible: tip.length > 0 && hovered
    ToolTip.text: tip
    ToolTip.delay: 600

    background: Rectangle {
        radius: control.height / 2
        color: control.checked ? Theme.accentSoft
             : control.down ? Qt.lighter(control.color, 1.4)
             : control.hovered ? Qt.lighter(control.color, 1.2) : control.color
        border.color: control.checked ? Theme.accent : Theme.border
    }
    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6
            Image {
                visible: control.hasIcon
                anchors.verticalCenter: parent.verticalCenter
                source: control.iconSource
                sourceSize: Qt.size(control.iconSize, control.iconSize)
                width: control.iconSize
                height: control.iconSize
            }
            Label {
                visible: control.text.length > 0
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                color: control.textColor
                font.pixelSize: 15
            }
        }
    }
}
