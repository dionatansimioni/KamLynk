import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

Rectangle {
    id: bar
    implicitHeight: 48
    color: Theme.panel

    signal aboutRequested()
    signal settingsRequested()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 12
        spacing: 20

        Label {
            text: Camera.modelName
            color: Theme.text
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        // Battery.
        RowLayout {
            spacing: 7
            ToolTip.visible: batteryHover.hovered
            ToolTip.text: qsTr("Bateria")
            HoverHandler { id: batteryHover }
            BatteryIcon {
                objectName: "batteryIcon"
                level: Camera.batteryPercent
            }
            Label {
                objectName: "batteryLabel"
                text: Camera.batteryPercent >= 0 ? Camera.batteryPercent + "%" : "--"
                color: Camera.batteryPercent >= 0 && Camera.batteryPercent <= 15 ? Theme.record : Theme.text
                font.features: { "tnum": 1 }
            }
        }

        MediaIndicator {}

        RowLayout {
            visible: Camera.overheating
            spacing: 4
            Image { source: Theme.icon("thermo"); sourceSize: Qt.size(18, 18) }
            Label { text: qsTr("Superaquecendo"); color: Theme.warn }
        }

        Item { Layout.fillWidth: true }
        Rectangle {
            visible: Camera.recording
            radius: 6
            color: Theme.record
            implicitWidth: recLabel.implicitWidth + 20
            implicitHeight: 28
            Label {
                id: recLabel
                anchors.centerIn: parent
                text: "● REC " + Camera.recordingTime
                color: "white"
                font.weight: Font.Bold
                font.features: { "tnum": 1 }
            }
        }
        Label {
            Layout.maximumWidth: 360
            elide: Text.ElideRight
            text: Camera.statusMessage
            color: Theme.textDim
        }
        IconButton {
            objectName: "settingsButton"
            iconSource: Theme.icon("settings")
            size: 34
            tip: qsTr("Configurações")
            onClicked: bar.settingsRequested()
        }
        IconButton {
            objectName: "aboutButton"
            iconSource: Theme.icon("info")
            size: 34
            tip: qsTr("Sobre o KamLynk")
            onClicked: bar.aboutRequested()
        }
        IconButton {
            text: qsTr("Desconectar")
            size: 34
            onClicked: Camera.disconnectCamera()
        }
    }
}
