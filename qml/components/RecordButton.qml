import QtQuick
import QtQuick.Controls
import KamLynk.Backend
import KamLynk

// Big start/stop movie recording button.
AbstractButton {
    id: button
    objectName: "recordButton"
    readonly property bool recording: Camera.recording
    implicitWidth: 76
    implicitHeight: 76
    hoverEnabled: true

    ToolTip.visible: hovered
    ToolTip.text: recording ? qsTr("Parar gravação (R)") : qsTr("Gravar vídeo (R)")
    ToolTip.delay: 600

    onClicked: Camera.toggleRecording()

    background: Rectangle {
        radius: width / 2
        color: "transparent"
        border.width: 4
        border.color: button.hovered ? "white" : "#d8d8d8"

        Rectangle {
            anchors.centerIn: parent
            width: button.recording ? parent.width * 0.38 : parent.width * 0.74
            height: width
            radius: button.recording ? 6 : width / 2
            color: Theme.record
            Behavior on width { NumberAnimation { duration: 140 } }
            Behavior on radius { NumberAnimation { duration: 140 } }
        }
    }
    contentItem: Item {}
}
