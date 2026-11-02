import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

// App preferences. Each change applies immediately and is remembered.
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

        Label {
            Layout.topMargin: 12
            text: qsTr("Depuração")
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Switch {
            objectName: "debugLogSwitch"
            Layout.fillWidth: true
            text: qsTr("Registrar log de depuração")
            checked: DebugLog.enabled
            onToggled: DebugLog.enabled = checked
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textDim
            font.pixelSize: 12
            text: qsTr("Grava o que o app faz em um arquivo, para enviar junto com relatos de problemas. "
                       + "Continua ligado depois de reabrir o app. Arquivo: %1").arg(DebugLog.filePath)
        }
        Button {
            objectName: "openLogFolder"
            visible: DebugLog.enabled
            flat: true
            text: qsTr("Abrir pasta do log")
            onClicked: DebugLog.openFolder()
        }
    }
}
