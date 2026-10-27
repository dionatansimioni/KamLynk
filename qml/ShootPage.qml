import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

Item {
    id: page
    objectName: "shootPage"

    property bool fullscreen: false
    signal aboutRequested()
    signal settingsRequested()
    signal fullscreenToggled()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StatusBar {
            Layout.fillWidth: true
            visible: !page.fullscreen
            onAboutRequested: page.aboutRequested()
            onSettingsRequested: page.settingsRequested()
        }

        SplitView {
            id: split
            objectName: "shootSplit"
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 6
                color: SplitHandle.pressed ? Theme.accent
                     : SplitHandle.hovered ? Theme.borderStrong : Theme.border
                Rectangle { // grip
                    anchors.centerIn: parent
                    width: 2; height: 28; radius: 1
                    color: SplitHandle.pressed ? "white" : Theme.textDim
                    opacity: 0.7
                }
            }

            // Live view + bottom controls.
            ColumnLayout {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 520
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    LiveView {
                        id: liveView
                        objectName: "liveView"
                        anchors.fill: parent
                        controller: Camera
                        mirrored: Camera.mirrorPreview
                    }

                    // Red frame while recording, like the camera's own screen.
                    Rectangle {
                        x: liveView.imageRect.x
                        y: liveView.imageRect.y
                        width: liveView.imageRect.width
                        height: liveView.imageRect.height
                        visible: Camera.recording && liveView.hasFrame
                        color: "transparent"
                        border.color: Theme.record
                        border.width: 3
                    }

                    // Rule-of-thirds grid.
                    Item {
                        x: liveView.imageRect.x
                        y: liveView.imageRect.y
                        width: liveView.imageRect.width
                        height: liveView.imageRect.height
                        visible: gridToggle.checked && liveView.hasFrame
                        Repeater {
                            model: 2
                            Rectangle {
                                required property int index
                                x: parent.width * (index + 1) / 3
                                width: 1; height: parent.height
                                color: "#66ffffff"
                            }
                        }
                        Repeater {
                            model: 2
                            Rectangle {
                                required property int index
                                y: parent.height * (index + 1) / 3
                                height: 1; width: parent.width
                                color: "#66ffffff"
                            }
                        }
                    }

                    // Touch-to-focus reticle.
                    Rectangle {
                        id: reticle
                        width: 72; height: 72
                        color: "transparent"
                        border.color: Theme.ok
                        border.width: 2
                        radius: 4
                        opacity: 0
                        NumberAnimation on opacity {
                            id: reticleFade
                            running: false
                            from: 1; to: 0; duration: 1600
                            easing.type: Easing.InQuad
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: Camera.touchFocusAvailable
                        cursorShape: enabled ? Qt.CrossCursor : Qt.ArrowCursor
                        onClicked: mouse => {
                            const p = liveView.normalizedPoint(mouse.x, mouse.y)
                            if (p.x < 0)
                                return
                            reticle.x = mouse.x - reticle.width / 2
                            reticle.y = mouse.y - reticle.height / 2
                            reticleFade.restart()
                            Camera.touchFocus(p.x, p.y)
                        }
                    }

                    // Recording refused or stopped by the camera.
                    Rectangle {
                        id: recordingAlert
                        objectName: "recordingAlert"
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: 16
                        width: Math.min(parent.width - 32, alertRow.implicitWidth + 32)
                        height: alertRow.implicitHeight + 20
                        radius: Theme.radius
                        color: Theme.record
                        visible: Camera.recordingError.length > 0
                        RowLayout {
                            id: alertRow
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 8
                            spacing: 12
                            Label {
                                Layout.fillWidth: true
                                Layout.maximumWidth: recordingAlert.parent.width - 120
                                text: Camera.recordingError
                                color: "white"
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                            }
                            ToolButton {
                                text: "✕"
                                onClicked: Camera.dismissRecordingError()
                            }
                        }
                    }

                    // Connection dropped: the session waits a few seconds before giving up.
                    Rectangle {
                        objectName: "reconnectingOverlay"
                        anchors.centerIn: parent
                        visible: Camera.reconnecting
                        width: reconnectRow.implicitWidth + 40
                        height: 56
                        radius: 28
                        color: "#d9111214"
                        border.color: Theme.borderStrong
                        RowLayout {
                            id: reconnectRow
                            anchors.centerIn: parent
                            spacing: 12
                            BusyIndicator { implicitWidth: 28; implicitHeight: 28; running: parent.visible }
                            Label { text: qsTr("Reconectando à câmera…"); color: Theme.text; font.pixelSize: 15 }
                        }
                    }

                    // Silent low battery notice (never interrupts recording).
                    Rectangle {
                        id: batteryToast
                        objectName: "lowBatteryToast"
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 16
                        anchors.topMargin: recordingAlert.visible ? recordingAlert.height + 28 : 16
                        visible: Camera.lowBattery
                        width: Math.min(380, parent.width - 32)
                        height: batteryRow.implicitHeight + 20
                        radius: Theme.radius
                        color: "#e61b1d21"
                        border.color: Theme.warn
                        RowLayout {
                            id: batteryRow
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 6
                            spacing: 10
                            Image { source: Theme.icon("battery-low"); sourceSize: Qt.size(24, 24) }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: Theme.text
                                text: qsTr("Bateria da câmera em %1%. Considere ligar a câmera na energia (USB-C) para não "
                                           + "interromper a gravação.").arg(Camera.batteryPercent)
                            }
                            ToolButton {
                                text: "✕"
                                onClicked: Camera.dismissLowBattery()
                            }
                        }
                    }

                    // Leave full screen.
                    IconButton {
                        objectName: "exitFullscreen"
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: 16
                        visible: page.fullscreen
                        opacity: hovered ? 1.0 : 0.55
                        color: "#cc1b1d21"
                        iconSource: Theme.icon("fullscreen-exit")
                        text: qsTr("Sair da tela cheia")
                        tip: qsTr("Voltar ao normal (Esc ou Ctrl+F11)")
                        onClicked: page.fullscreenToggled()
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: !liveView.hasFrame && !Camera.reconnecting
                        text: qsTr("Aguardando imagem da câmera…")
                        color: Theme.textDim
                        font.pixelSize: 16
                    }
                }

                // Bottom bar: preview tools | capture | last photo. The two side areas share
                // the free width equally so the capture buttons stay centered.
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 104
                    visible: !page.fullscreen
                    color: Theme.panel

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 16

                        RowLayout {
                            id: previewTools
                            Layout.fillWidth: true
                            // Both sides prefer the same width, so the capture buttons stay
                            // centered; the tools are never squeezed under them (narrow windows
                            // move the buttons right instead).
                            Layout.preferredWidth: implicitWidth
                            Layout.minimumWidth: implicitWidth
                            spacing: 8
                            ZoomRocker {}
                            IconButton {
                                id: gridToggle
                                objectName: "gridToggle"
                                checkable: true
                                iconSource: Theme.icon("grid")
                                tip: qsTr("Grade (regra dos terços)")
                            }
                            IconButton {
                                objectName: "fullscreenButton"
                                iconSource: Theme.icon("fullscreen")
                                tip: qsTr("Prévia em tela cheia (Ctrl+F11)")
                                onClicked: page.fullscreenToggled()
                            }
                            IconButton {
                                objectName: "mirrorToggle"
                                checkable: true
                                checked: Camera.mirrorPreview
                                iconSource: Theme.icon("mirror")
                                tip: qsTr("Espelhar a prévia (o vídeo gravado não muda)")
                                onToggled: Camera.mirrorPreview = checked
                            }
                            ColumnLayout {
                                visible: Camera.liveViewQuality.available
                                spacing: 0
                                ComboBox {
                                    id: qualityBox
                                    objectName: "previewQuality"
                                    implicitWidth: 104
                                    implicitHeight: 40
                                    topInset: 0
                                    bottomInset: 0
                                    font.pixelSize: 13
                                    enabled: Camera.liveViewQuality.writable
                                    model: Camera.liveViewQuality.options
                                    currentIndex: Camera.liveViewQuality.index
                                    displayText: Camera.liveViewQuality.text
                                    onActivated: index => Camera.liveViewQuality.setIndex(index)
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Qualidade da transmissão da prévia (Alta: 1024 px, Baixa: 640 px, mais fluida)")
                                    ToolTip.delay: 600
                                }
                                Label {
                                    objectName: "previewStats"
                                    Layout.alignment: Qt.AlignHCenter
                                    visible: liveView.hasFrame
                                    text: qsTr("Prévia · %1×%2 · %3 fps").arg(Camera.frameSize.width)
                                          .arg(Camera.frameSize.height).arg(Camera.liveViewFps)
                                    color: Theme.textDim
                                    font.pixelSize: 11
                                    font.features: { "tnum": 1 }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            spacing: 18
                            RecordButton {}
                            AbstractButton {
                                id: photoButton
                                objectName: "photoButton"
                                implicitWidth: 60
                                implicitHeight: 60
                                hoverEnabled: true
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Tirar foto (Espaço)")
                                ToolTip.delay: 600
                                onClicked: Camera.capturePhoto()
                                background: Rectangle {
                                    radius: width / 2
                                    color: "transparent"
                                    border.width: 3
                                    border.color: photoButton.hovered ? "white" : "#d8d8d8"
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: parent.width * (photoButton.down ? 0.66 : 0.76)
                                        height: width
                                        radius: width / 2
                                        color: "white"
                                        Image {
                                            anchors.centerIn: parent
                                            source: Theme.icon("camera")
                                            sourceSize: Qt.size(24, 24)
                                        }
                                    }
                                }
                                contentItem: Item {}
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: previewTools.implicitWidth
                            spacing: 12
                            Item { Layout.fillWidth: true }
                            Image {
                                Layout.preferredWidth: 96
                                Layout.preferredHeight: 64
                                visible: Camera.lastDownload.length > 0
                                source: Camera.lastDownload.length > 0 ? "file://" + Camera.lastDownload : ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Qt.openUrlExternally("file://" + Camera.lastDownload)
                                }
                            }
                        }
                    }
                }
            }

            // Settings side panel (resizable; width remembered).
            Rectangle {
                id: sidePanel
                objectName: "sidePanel"
                visible: !page.fullscreen
                SplitView.preferredWidth: Camera.sidePanelWidth
                SplitView.minimumWidth: 280
                SplitView.maximumWidth: 560
                color: Theme.panel

                ScrollView {
                    id: scroll
                    anchors.fill: parent
                    anchors.topMargin: 14
                    anchors.bottomMargin: 14
                    leftPadding: 14
                    // Room for the scroll bar so it never covers the controls.
                    rightPadding: 14 + (ScrollBar.vertical.visible ? ScrollBar.vertical.width : 0)
                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: scroll.availableWidth
                        spacing: 8

                        SectionHeader { text: qsTr("EXPOSIÇÃO"); Layout.topMargin: 0 }
                        OptionPicker { prop: Camera.exposureMode }
                        GridLayout {
                            objectName: "exposureGrid"
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8
                            SettingTile { prop: Camera.fNumber }
                            SettingTile { prop: Camera.shutterSpeed }
                            SettingTile { prop: Camera.iso }
                            SettingTile { prop: Camera.exposureBias }
                        }

                        SectionHeader { text: qsTr("FOCO") }
                        FocusPanel {}

                        SectionHeader { text: qsTr("COR E ESTILO") }
                        OptionPicker { prop: Camera.whiteBalance }
                        SettingTile {
                            prop: Camera.colorTemp
                            visible: Camera.colorTemp.available && Camera.colorTempMode
                        }
                        OptionPicker { prop: Camera.creativeLook }
                        OptionPicker { prop: Camera.pictureProfile }
                        OptionPicker { prop: Camera.cinematicVlog }

                        SectionHeader { text: qsTr("VÍDEO") }
                        OptionPicker { prop: Camera.movieFileFormat }
                        OptionPicker { prop: Camera.recordingSetting }
                        OptionPicker { prop: Camera.frameRate }
                        OptionPicker { prop: Camera.audioRecording }
                        SettingTile {
                            prop: Camera.audioLevel
                            enabled: Camera.audioEnabled
                        }

                        SectionHeader { text: qsTr("TRANSFERÊNCIA") }
                        OptionPicker { prop: Camera.saveDestination }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                            color: Theme.textDim
                            font.pixelSize: 12
                            text: qsTr("Fotos em: %1").arg(Camera.downloadDirectory)
                        }
                    }
                }
            }

            onResizingChanged: {
                if (!resizing)
                    Camera.sidePanelWidth = Math.round(sidePanel.width)
            }
        }
    }
}
