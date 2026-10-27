import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

Item {
    id: page
    objectName: "connectPage"

    signal aboutRequested()
    signal settingsRequested()

    readonly property bool busy: Camera.state === CameraController.Discovering
                                 || Camera.state === CameraController.Connecting

    RowLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 32

        // Left: discovery and manual connection.
        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: false
            Layout.preferredWidth: 520
            Layout.maximumWidth: 560
            spacing: 18

            Label {
                text: "KamLynk"
                font.pixelSize: 40
                font.weight: Font.Bold
                color: Theme.text
            }
            Label {
                text: qsTr("Controle remoto para câmeras Sony")
                color: Theme.textDim
                font.pixelSize: 16
            }

            RowLayout {
                Layout.topMargin: 16
                spacing: 12
                Button {
                    objectName: "discoverButton"
                    text: qsTr("Procurar câmeras")
                    highlighted: true
                    enabled: !page.busy
                    onClicked: Camera.discover()
                }
                BusyIndicator {
                    running: page.busy
                    visible: running
                    implicitWidth: 36
                    implicitHeight: 36
                }
            }

            // Known cameras (history) first, then cameras found on the network.
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 180
                radius: Theme.radius
                color: Theme.panel
                border.color: Theme.border

                ListView {
                    id: cameraList
                    objectName: "cameraList"
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    spacing: 6
                    model: Camera.devices
                    section.property: "known"
                    section.delegate: Label {
                        required property string section
                        width: ListView.view.width
                        topPadding: 6
                        bottomPadding: 4
                        leftPadding: 6
                        text: section === "true" ? qsTr("MINHAS CÂMERAS") : qsTr("ENCONTRADAS NA REDE")
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.2
                        color: Theme.textDim
                    }
                    delegate: ItemDelegate {
                        id: row
                        required property var modelData
                        required property int index
                        objectName: "device_" + index
                        width: ListView.view.width
                        height: 68
                        enabled: Camera.state !== CameraController.Connecting
                        onClicked: if (modelData.canConnect) Camera.connectDevice(modelData.key)
                        background: Rectangle {
                            radius: 8
                            color: row.hovered ? Theme.panelRaised : "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: 14
                            Rectangle {
                                width: 40; height: 40; radius: 20
                                color: Theme.panelRaised
                                Label {
                                    anchors.centerIn: parent
                                    text: row.modelData.connection === "USB" ? "USB" : "Wi-Fi"
                                    font.pixelSize: 10
                                    color: Theme.accent
                                }
                                // Online status dot.
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    width: 12; height: 12; radius: 6
                                    color: row.modelData.online ? Theme.ok : Theme.textDim
                                    border.color: Theme.panel
                                    border.width: 2
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                RowLayout {
                                    spacing: 8
                                    Label { text: row.modelData.model; font.pixelSize: 17; color: Theme.text }
                                    Label {
                                        objectName: "deviceStatus"
                                        text: row.modelData.online ? qsTr("Online") : qsTr("Offline")
                                        font.pixelSize: 12
                                        color: row.modelData.online ? Theme.ok : Theme.textDim
                                    }
                                    Image {
                                        visible: row.modelData.savedLogin
                                        source: Theme.icon("key")
                                        sourceSize: Qt.size(15, 15)
                                        ToolTip.visible: keyHover.hovered
                                        ToolTip.text: qsTr("Login salvo no chaveiro do sistema")
                                        HoverHandler { id: keyHover }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: row.modelData.connection + " · " + row.modelData.address
                                          + (row.modelData.lastUsed ? " · " + qsTr("usada %1").arg(row.modelData.lastUsed) : "")
                                    color: Theme.textDim
                                }
                            }
                            Label {
                                visible: row.modelData.canConnect
                                text: qsTr("Conectar ›")
                                color: Theme.accent
                            }
                            IconButton {
                                objectName: "forgetDevice"
                                visible: row.modelData.known
                                text: "✕"
                                size: 30
                                tip: qsTr("Esquecer esta câmera (e o login salvo)")
                                onClicked: Camera.forgetDevice(row.modelData.key)
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: cameraList.count === 0
                        width: parent.width - 40
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: Theme.textDim
                        text: page.busy ? qsTr("Procurando…") : qsTr("Nenhuma câmera encontrada ainda.")
                    }
                }
            }

            Label { text: qsTr("Conectar por IP"); color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
            RowLayout {
                spacing: 10
                TextField {
                    id: ipField
                    objectName: "ipField"
                    Layout.preferredWidth: 180
                    placeholderText: "192.168.0.10"
                    inputMethodHints: Qt.ImhPreferNumbers
                }
                ComboBox {
                    id: modelBox
                    Layout.preferredWidth: 180
                    model: Camera.supportedModels
                    currentIndex: Camera.defaultModelIndex
                }
                Button {
                    text: qsTr("Conectar")
                    enabled: !page.busy && ipField.text.length > 6
                    onClicked: Camera.connectToAddress(ipField.text, modelBox.currentIndex)
                }
            }

            RowLayout {
                visible: Camera.usb.available && !Camera.usb.installed
                spacing: 10
                Button {
                    objectName: "usbSetupButton"
                    text: qsTr("Liberar acesso USB…")
                    enabled: !Camera.usb.busy
                    onClicked: Camera.usb.install()
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.textDim
                    font.pixelSize: 12
                    text: qsTr("Necessário só para conexão por cabo. Pede a senha de administrador uma vez.")
                }
            }

            Label {
                objectName: "statusLabel"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.textDim
                text: Camera.statusMessage
            }
            RowLayout {
                spacing: 10
                IconButton {
                    objectName: "connectSettingsButton"
                    iconSource: Theme.icon("settings")
                    text: qsTr("Configurações")
                    size: 34
                    onClicked: page.settingsRequested()
                }
                IconButton {
                    objectName: "connectAboutButton"
                    iconSource: Theme.icon("info")
                    text: qsTr("Sobre")
                    size: 34
                    onClicked: page.aboutRequested()
                }
            }
        }

        // Right: how to prepare the camera.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.panel
            border.color: Theme.border

            ScrollView {
                id: helpScroll
                anchors.fill: parent
                anchors.margins: 24
                contentWidth: availableWidth
                clip: true
                Label {
                    width: helpScroll.availableWidth
                    wrapMode: Text.Wrap
                    textFormat: Text.MarkdownText
                    color: Theme.text
                    text: qsTr("### Preparando a ZV-E10 II\n\n" +
                               "#### Wi-Fi pelo roteador (recomendado)\n\n" +
                               "1. MENU › Rede › Wi-Fi › *Conexão do ponto de acesso*: conecte a câmera à mesma rede deste PC.\n" +
                               "2. MENU › Rede › Conex./Disp. remoto › *Função de disparo remoto* › **Disparo remoto: Ligado**.\n" +
                               "3. Clique em *Procurar câmeras* e depois na câmera encontrada.\n" +
                               "4. **Com Autent. de acesso ligada** (padrão em Rede › Opção de rede › Config. autent. acesso): " +
                               "o app pede o usuário e a senha mostrados nessa tela da câmera.\n" +
                               "5. **Com Autent. de acesso desligada**: antes de conectar, inicie o **Emparelhamento** " +
                               "(Rede › Conex./Disp. remoto › Função de disparo remoto) e confirme **OK** na câmera.\n\n" +
                               "#### Wi-Fi Direct (sem roteador)\n\n" +
                               "Na mesma tela, escolha *Wi-Fi Direct*, conecte este PC à rede Wi-Fi mostrada pela câmera e procure de novo.\n\n" +
                               "#### USB\n\n" +
                               "MENU › Rede › Conex./Disp. remoto › *Método de conexão*: **USB**. Ligue o cabo e procure.\n\n" +
                               "#### Dicas\n\n" +
                               "- Mantenha a câmera a poucos metros do roteador e prefira 5 GHz.\n" +
                               "- Desligue o Bluetooth da câmera para uma prévia mais fluida.\n" +
                               "- Os nomes dos menus podem variar um pouco conforme o idioma da câmera.")
                }
            }
        }
    }
}
