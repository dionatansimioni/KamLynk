import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

// Memory card in the status bar: usage bar (when the card size is known), estimated free
// space and recording time left. Click for details and to set the card size.
AbstractButton {
    id: control
    objectName: "mediaIndicator"
    hoverEnabled: true
    implicitHeight: 32
    implicitWidth: row.implicitWidth + 16

    readonly property bool cardOk: Camera.mediaOk
    readonly property real used: Camera.mediaUsedFraction
    readonly property color barColor: used > 0.9 ? Theme.record : used > 0.75 ? Theme.warn : Theme.accent

    function gb(value) {
        return (value >= 100 ? Math.round(value) : Number(value).toLocaleString(Qt.locale(), "f", 1)) + " GB"
    }

    onClicked: details.opened ? details.close() : details.open()

    background: Rectangle {
        radius: 8
        color: control.hovered || details.opened ? Theme.panelRaised : "transparent"
    }

    contentItem: Item {}
    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 7
        Image {
            source: Theme.icon("sdcard")
            sourceSize: Qt.size(18, 18)
        }
        // Mini usage bar.
        Rectangle {
            visible: control.used >= 0
            implicitWidth: 34
            implicitHeight: 6
            radius: 3
            color: Theme.border
            Rectangle {
                width: parent.width * Math.max(0.04, control.used)
                height: parent.height
                radius: 3
                color: control.barColor
            }
        }
        Label {
            objectName: "mediaLabel"
            text: !control.cardOk ? Camera.mediaStatus
                 : Camera.mediaFreeGb >= 0 ? qsTr("≈%1 livres · %2").arg(control.gb(Camera.mediaFreeGb)).arg(Camera.mediaRemaining)
                 : Camera.mediaRemaining
            color: control.cardOk ? Theme.text : Theme.warn
            font.features: { "tnum": 1 }
        }
    }

    Popup {
        id: details
        objectName: "mediaDetails"
        y: control.height + 6
        width: 300
        padding: 16
        background: Rectangle {
            radius: Theme.radius
            color: Theme.panelRaised
            border.color: Theme.borderStrong
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                text: qsTr("Cartão de memória")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: Theme.text
            }
            // Large usage bar with legend.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                visible: control.used >= 0
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 10
                    radius: 5
                    color: Theme.border
                    Rectangle {
                        width: parent.width * Math.max(0.02, control.used)
                        height: parent.height
                        radius: 5
                        color: control.barColor
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Usado ≈%1").arg(control.gb(Camera.cardCapacityGb * Math.max(0, control.used)))
                        color: Theme.textDim
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: Math.round(control.used * 100) + "%"
                        color: Theme.textDim
                        font.pixelSize: 12
                    }
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 6
                Label { text: qsTr("Situação"); color: Theme.textDim }
                Label { text: Camera.mediaStatus || "--"; color: control.cardOk ? Theme.text : Theme.warn }
                Label { text: qsTr("Livre (estimado)"); color: Theme.textDim }
                Label { text: Camera.mediaFreeGb >= 0 ? "≈" + control.gb(Camera.mediaFreeGb) : "--"; color: Theme.text }
                Label { text: qsTr("Vídeo restante"); color: Theme.textDim }
                Label { text: Camera.mediaRemaining; color: Theme.text }
                Label { text: qsTr("Fotos restantes"); color: Theme.textDim }
                Label { text: Camera.mediaPhotosLeft >= 0 ? Camera.mediaPhotosLeft : "--"; color: Theme.text }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Capacidade do cartão")
                color: Theme.textDim
                font.pixelSize: 12
            }
            ComboBox {
                id: capacityBox
                objectName: "cardCapacity"
                Layout.fillWidth: true
                implicitHeight: 40
                topInset: 0
                bottomInset: 0
                readonly property var sizes: [0, 32, 64, 128, 256, 512, 1024]
                model: [qsTr("Não informada"), "32 GB", "64 GB", "128 GB", "256 GB", "512 GB", "1 TB"]
                currentIndex: Math.max(0, sizes.indexOf(Camera.cardCapacityGb))
                onActivated: index => Camera.cardCapacityGb = sizes[index]
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: Theme.textDim
                text: qsTr("A câmera não informa a capacidade do cartão ao app: o espaço livre é estimado pelo " +
                           "tempo de vídeo restante na qualidade de gravação atual. Informe o tamanho do cartão " +
                           "para ver o espaço usado.")
            }
        }
    }
}
