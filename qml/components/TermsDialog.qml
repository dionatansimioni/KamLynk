import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

// Shown on first start until accepted: the Sony Camera Remote SDK license requires
// end users to be told about (and agree to) the warranty and prohibited-use terms.
Dialog {
    id: dialog
    objectName: "termsDialog"
    title: qsTr("Antes de começar")
    modal: true
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - 48 : 560)
    footer: DialogButtonBox {
        Button {
            objectName: "declineTerms"
            text: qsTr("Recusar e sair")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            objectName: "acceptTerms"
            text: qsTr("Aceitar")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
    onAccepted: Camera.acceptTerms()
    onRejected: Qt.quit()

    ColumnLayout {
        anchors.fill: parent

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.MarkdownText
            color: Theme.text
            linkColor: Theme.accent
            onLinkActivated: link => Qt.openUrlExternally(link)
            text: qsTr("O KamLynk é um software livre e gratuito (licença MIT), fornecido **sem nenhuma garantia**. " +
                       "Ele **não é feito, apoiado nem endossado pela Sony**.\n\n" +
                       "Para controlar a câmera, ele usa o Camera Remote SDK da Sony. Os termos desse SDK pedem " +
                       "que você saiba e concorde que:\n\n" +
                       "- ao ser controlada por este aplicativo, a câmera pode ficar **fora da garantia do " +
                       "fabricante**, conforme especificado pela Sony (salvo onde a lei local não permitir);\n" +
                       "- este aplicativo serve apenas para controlar e usar a câmera remotamente, em uso normal;\n" +
                       "- é **proibido** usá-lo em atividades militares ou relacionadas a armas.\n\n" +
                       "Licenças e avisos completos: " +
                       "[github.com/dionatansimioni/KamLynk](https://github.com/dionatansimioni/KamLynk)")
        }
    }
}
