import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import KamLynk.Backend
import KamLynk

ApplicationWindow {
    id: window
    width: 1360
    height: 820
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: Camera.state === CameraController.Connected ? "KamLynk — " + Camera.modelName : "KamLynk"
    color: Theme.background

    Material.theme: Material.Dark
    Material.accent: Theme.accent
    Material.background: Theme.background

    readonly property bool connected: Camera.state === CameraController.Connected

    // Preview-only full screen (Ctrl+F11); restores the previous window state on exit.
    property bool previewFullscreen: false
    property int visibilityBeforeFullscreen: Window.Windowed
    function setPreviewFullscreen(on) {
        if (on === previewFullscreen)
            return
        if (on) {
            visibilityBeforeFullscreen = window.visibility
            window.showFullScreen()
        } else {
            window.visibility = visibilityBeforeFullscreen === Window.FullScreen ? Window.Windowed
                                                                               : visibilityBeforeFullscreen
        }
        previewFullscreen = on
    }
    onConnectedChanged: if (!connected) setPreviewFullscreen(false)

    Loader {
        id: pageLoader
        objectName: "pageLoader"
        anchors.fill: parent
        sourceComponent: window.connected ? shootPage : connectPage
    }

    Component {
        id: connectPage
        ConnectPage {
            onAboutRequested: about.open()
            onSettingsRequested: settings.open()
        }
    }
    Component {
        id: shootPage
        ShootPage {
            fullscreen: window.previewFullscreen
            onAboutRequested: about.open()
            onSettingsRequested: settings.open()
            onFullscreenToggled: window.setPreviewFullscreen(!window.previewFullscreen)
        }
    }

    CredentialsDialog { id: credentials }
    AboutDialog { id: about }
    SettingsDialog { id: settings }
    TermsDialog { id: terms }
    Component.onCompleted: if (!Camera.termsAccepted) terms.open()

    Connections {
        target: Camera
        function onCredentialsRequested() { credentials.open() }
    }

    // Global shortcuts, like the camera body buttons.
    Shortcut {
        sequence: "R"
        enabled: window.connected
        onActivated: Camera.toggleRecording()
    }
    Shortcut {
        sequence: "Ctrl+F11"
        enabled: window.connected
        onActivated: window.setPreviewFullscreen(!window.previewFullscreen)
    }
    Shortcut {
        sequence: "Esc"
        enabled: window.previewFullscreen
        onActivated: window.setPreviewFullscreen(false)
    }
    Shortcut {
        sequence: "Space"
        enabled: window.connected
        onActivated: Camera.capturePhoto()
    }
}
