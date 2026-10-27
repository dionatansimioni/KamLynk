import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KamLynk.Backend
import KamLynk

// Hold W/T to zoom (power zoom lenses / Clear Image Zoom), release to stop.
RowLayout {
    visible: Camera.zoomAvailable
    spacing: 6

    IconButton {
        text: "W"
        tip: qsTr("Zoom out (segure)")
        onPressed: Camera.zoom(-1)
        onReleased: Camera.zoom(0)
        onCanceled: Camera.zoom(0)
    }
    IconButton {
        text: "T"
        tip: qsTr("Zoom in (segure)")
        onPressed: Camera.zoom(1)
        onReleased: Camera.zoom(0)
        onCanceled: Camera.zoom(0)
    }
}
