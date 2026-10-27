import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

Dialog {
    id: dialog
    objectName: "credentialsDialog"
    title: qsTr("Autenticação SSH da câmera")
    modal: true
    anchors.centerIn: parent
    width: 460
    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancelar")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: qsTr("Conectar")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        Camera.submitCredentials(user.text, password.text, remember.checked)
        password.text = ""
    }
    onRejected: Camera.cancelCredentials()
    onOpened: {
        reveal.checked = false
        password.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        // Wrong password / rejected saved login.
        Rectangle {
            objectName: "credentialsError"
            Layout.fillWidth: true
            visible: Camera.credentialsError.length > 0
            implicitHeight: errorLabel.implicitHeight + 16
            radius: 8
            color: "#33e5322d"
            border.color: Theme.record
            Label {
                id: errorLabel
                anchors.fill: parent
                anchors.margins: 8
                wrapMode: Text.Wrap
                color: Theme.text
                text: Camera.credentialsError
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textDim
            text: qsTr("Confira se a impressão digital abaixo é a mesma mostrada na câmera " +
                       "(Rede › Opção de rede › Config. autent. acesso).")
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WrapAnywhere
            font.family: "monospace"
            text: Camera.pendingFingerprint
        }
        TextField {
            id: user
            Layout.fillWidth: true
            placeholderText: qsTr("Usuário")
            text: "admin"
        }
        TextField {
            id: password
            objectName: "passwordField"
            Layout.fillWidth: true
            placeholderText: qsTr("Senha")
            echoMode: reveal.checked ? TextInput.Normal : TextInput.Password
            rightPadding: reveal.width + 12
            onAccepted: dialog.accept()

            // Show/hide the typed password.
            ToolButton {
                id: reveal
                objectName: "revealPassword"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                checkable: true
                focusPolicy: Qt.NoFocus
                icon.source: checked ? Theme.icon("eye-off") : Theme.icon("eye")
                icon.color: Theme.textDim
                icon.width: 20
                icon.height: 20
                ToolTip.visible: hovered
                ToolTip.text: checked ? qsTr("Ocultar senha") : qsTr("Mostrar senha")
                ToolTip.delay: 500
            }
        }
        CheckBox {
            id: remember
            objectName: "rememberLogin"
            visible: Camera.credentialStoreAvailable
            checked: true
            text: qsTr("Lembrar usuário e senha (no chaveiro do sistema)")
        }
    }
}
