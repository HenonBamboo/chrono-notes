import QtQuick
import QtQuick.Controls

Button {
    id: control

    readonly property ChronoTokens tokens: ChronoTokens {}

    property string label: ""
    property bool danger: false

    width: 32
    height: 28
    hoverEnabled: true

    contentItem: Text {
        text: control.label
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: control.danger ? tokens.danger : tokens.muted
        font.pixelSize: 14
        font.family: tokens.fontUi
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        radius: tokens.radiusSm
        color: control.hovered ? (control.danger ? tokens.dangerSoft : tokens.paperSoft) : "transparent"
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
