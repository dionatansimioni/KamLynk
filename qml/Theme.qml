pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#111214"
    readonly property color panel: "#1b1d21"
    readonly property color panelRaised: "#24272c"
    readonly property color border: "#2f333a"
    readonly property color borderStrong: "#454a53"
    readonly property color accentSoft: "#33ff7a1a"
    readonly property color text: "#eceef1"
    readonly property color textDim: "#9097a1"
    readonly property color accent: "#ff7a1a"
    readonly property color record: "#e5322d"
    readonly property color ok: "#3ecf8e"
    readonly property color warn: "#ffb020"

    // Absolute URL of a bundled icon (qml/icons/<name>.svg), valid from any component.
    function icon(name) { return "qrc:/qt/qml/KamLynk/qml/icons/" + name + ".svg" }

    readonly property int radius: 10
    readonly property int spacing: 12
}
