import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

// One stepped setting (aperture, ISO, mic level…) as a tile: caption, large centered value
// (click for the full list, wheel to step) and a two-segment stepper. Exposure tiles sit in
// a 2-column grid; elsewhere a tile spans the panel. Hover shows what the setting does.
Rectangle {
    id: tile
    required property CameraProperty prop
    property string caption: prop ? prop.name : ""

    visible: prop && prop.available
    Layout.fillWidth: true
    Layout.preferredWidth: 1 // equal columns in the grid
    implicitHeight: 104
    radius: Theme.radius
    color: Theme.panelRaised
    border.color: hover.hovered ? Theme.borderStrong : Theme.border
    opacity: prop && prop.writable ? 1.0 : 0.5

    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && tile.prop && tile.prop.description.length > 0
    ToolTip.text: tile.prop ? tile.prop.description : ""
    ToolTip.delay: 700
    WheelHandler {
        enabled: tile.prop && tile.prop.writable
        onWheel: event => tile.prop.stepBy(event.angleDelta.y > 0 ? 1 : -1)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 2

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: tile.caption.toUpperCase()
            elide: Text.ElideRight
            color: Theme.textDim
            font.pixelSize: 10
            font.weight: Font.DemiBold
            font.letterSpacing: 1
        }
        Label {
            id: valueLabel
            objectName: "tileValue"
            Layout.fillWidth: true
            Layout.fillHeight: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: tile.prop ? tile.prop.text : "--"
            elide: Text.ElideRight
            color: Theme.text
            font.pixelSize: 21
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
            MouseArea {
                anchors.fill: parent
                enabled: tile.prop && tile.prop.writable && tile.prop.options.length > 0
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: menu.popup(valueLabel, 0, valueLabel.height)
            }
        }
        // Segmented stepper: two equal halves.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 28
            radius: 8
            color: Theme.panel
            border.color: Theme.border
            RowLayout {
                anchors.fill: parent
                spacing: 0
                Repeater {
                    model: [ { glyph: "‹", step: -1 }, { glyph: "›", step: 1 } ]
                    AbstractButton {
                        id: stepButton
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        enabled: tile.prop && (modelData.step < 0 ? tile.prop.canDecrease : tile.prop.canIncrease)
                        hoverEnabled: true
                        onClicked: tile.prop.stepBy(modelData.step)
                        background: Rectangle {
                            radius: 8
                            color: stepButton.down ? Theme.border
                                 : stepButton.hovered ? Theme.panelRaised : "transparent"
                        }
                        contentItem: Label {
                            text: stepButton.modelData.glyph
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 18
                            color: stepButton.enabled ? Theme.text : Theme.textDim
                            opacity: stepButton.enabled ? 1 : 0.4
                        }
                    }
                }
            }
            Rectangle { // divider
                anchors.centerIn: parent
                width: 1
                height: parent.height - 10
                color: Theme.border
            }
        }
    }

    Menu {
        id: menu
        height: Math.min(implicitHeight, 420)
        Repeater {
            model: tile.prop ? tile.prop.options : []
            MenuItem {
                required property string modelData
                required property int index
                text: modelData
                checkable: true
                checked: tile.prop && index === tile.prop.index
                onTriggered: tile.prop.setIndex(index)
            }
        }
    }
}
