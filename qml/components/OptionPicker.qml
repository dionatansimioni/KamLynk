import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

// Caption above a full-width drop-down (inspector style), for enumerated settings
// (WB, focus mode, Creative Look...). Stacking avoids truncated labels at any panel width.
ColumnLayout {
    id: picker
    required property CameraProperty prop
    property string label: prop ? prop.name : ""

    visible: prop && prop.available
    Layout.fillWidth: true
    spacing: 4

    Label {
        Layout.fillWidth: true
        text: picker.label
        color: Theme.textDim
        font.pixelSize: 12
        elide: Text.ElideRight
    }
    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && picker.prop && picker.prop.description.length > 0
    ToolTip.text: picker.prop ? picker.prop.description : ""
    ToolTip.delay: 700

    ComboBox {
        Layout.fillWidth: true
        implicitHeight: 40
        topInset: 0
        bottomInset: 0
        enabled: picker.prop && picker.prop.writable
        model: picker.prop ? picker.prop.options : []
        currentIndex: picker.prop ? picker.prop.index : -1
        displayText: picker.prop ? picker.prop.text : "--"
        onActivated: index => picker.prop.setIndex(index)
    }
}
