import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk

Dialog {
    id: dialog
    objectName: "aboutDialog"
    modal: true
    anchors.centerIn: parent
    width: Math.min(460, parent ? parent.width - 48 : 460)
    padding: 28
    footer: DialogButtonBox {
        Button {
            text: qsTr("Fechar")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/packaging/kamlynk.svg"
            sourceSize: Qt.size(96, 96)
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 6
            text: "KamLynk"
            font.pixelSize: 26
            font.weight: Font.Bold
            color: Theme.text
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Versão %1").arg(Qt.application.version.length > 0 ? Qt.application.version : "dev")
            color: Theme.textDim
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 10
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.text
            text: qsTr("Controle remoto para câmeras Sony no Linux: prévia ao vivo, gravação, exposição, foco e transferência de fotos.")
        }

        Rectangle { Layout.fillWidth: true; Layout.topMargin: 14; height: 1; color: Theme.border }

        GridLayout {
            Layout.fillWidth: true
            Layout.topMargin: 10
            columns: 2
            columnSpacing: 16
            rowSpacing: 8
            Label { text: qsTr("Criado por"); color: Theme.textDim }
            Label { text: "Dionatan Simioni"; color: Theme.text; Layout.fillWidth: true }
            Label { text: qsTr("Desenvolvimento"); color: Theme.textDim }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Com o auxílio do Claude Code (Anthropic)")
                color: Theme.text
            }
            Label { text: qsTr("Feito com"); color: Theme.textDim }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Qt 6 e Sony Camera Remote SDK")
                color: Theme.text
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 12
            wrapMode: Text.Wrap
            font.pixelSize: 11
            color: Theme.textDim
            text: qsTr("Camera Remote SDK © Sony Group Corporation, usado conforme os termos da Sony. " +
                       "Sony, ZV-E10 e marcas relacionadas pertencem aos seus donos; este app não é afiliado à Sony.")
        }
    }
}
