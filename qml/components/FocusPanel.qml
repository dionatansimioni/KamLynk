import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

ColumnLayout {
    spacing: 8
    Layout.fillWidth: true

    OptionPicker { prop: Camera.focusMode }
    OptionPicker { prop: Camera.focusArea }
    OptionPicker { prop: Camera.subjectRecognition }

    Label {
        Layout.fillWidth: true
        visible: Camera.touchFocusAvailable
        wrapMode: Text.Wrap
        color: Theme.textDim
        font.pixelSize: 12
        text: qsTr("Clique na imagem para focar naquele ponto.")
    }
    IconButton {
        Layout.fillWidth: true
        visible: Camera.touchFocusAvailable
        text: qsTr("Cancelar foco por toque")
        size: 32
        onClicked: Camera.cancelTouchFocus()
    }

    Label {
        visible: Camera.manualFocusAvailable
        text: qsTr("Foco manual (perto ↔ longe)")
        color: Theme.textDim
        font.pixelSize: 12
    }
    RowLayout {
        visible: Camera.manualFocusAvailable
        Layout.fillWidth: true
        spacing: 4
        Repeater {
            model: [
                { label: "‹‹‹", step: -7 }, { label: "‹‹", step: -3 }, { label: "‹", step: -1 },
                { label: "›", step: 1 }, { label: "››", step: 3 }, { label: "›››", step: 7 }
            ]
            IconButton {
                required property var modelData
                Layout.fillWidth: true
                text: modelData.label
                size: 34
                tip: modelData.step < 0 ? qsTr("Mais perto") : qsTr("Mais longe")
                onClicked: Camera.focusStep(modelData.step)
            }
        }
    }
}
