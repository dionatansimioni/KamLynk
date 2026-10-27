import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

// App preferences. The language applies immediately and is remembered.
Dialog {
    id: dialog
    objectName: "settingsDialog"
    title: qsTr("Configurações")
    modal: true
    anchors.centerIn: parent
    width: Math.min(420, parent ? parent.width - 48 : 420)
    footer: DialogButtonBox {
        Button {
            text: qsTr("Fechar")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            text: qsTr("Idioma")
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: Theme.text
        }
        ButtonGroup { id: languageGroup }
        Repeater {
            model: Language.options
            RadioButton {
                required property var modelData
                objectName: "language_" + (modelData.code || "system")
                Layout.fillWidth: true
                text: modelData.name
                checked: Language.choice === modelData.code
                ButtonGroup.group: languageGroup
                onClicked: Language.choice = modelData.code
            }
        }
    }
}
