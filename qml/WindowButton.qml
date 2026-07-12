import QtQuick
import QtQuick.Controls

Button {
    id: control

    readonly property ChronoTokens fallbackTokens: ChronoTokens {}
    readonly property var tokens: control.theme ? control.theme : control.fallbackTokens

    property string label: ""
    property bool danger: false
    property var theme: null
    property string uiFontFamily: control.tokens.fontUi
    property int uiFontSize: control.tokens.sizeBody

    width: 32
    height: 28
    hoverEnabled: true

    contentItem: Text {
        text: control.label
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: control.danger ? control.tokens.danger : control.tokens.muted
        font.pixelSize: Math.max(12, control.uiFontSize + 2)
        font.family: control.uiFontFamily
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        radius: control.tokens.radiusSm
        color: control.hovered ? (control.danger ? control.tokens.dangerSoft : control.tokens.paperSoft) : "transparent"
        Behavior on color { ColorAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic } }
    }

    scale: control.pressed ? 0.96 : (control.hovered ? 1.04 : 1)
    Behavior on scale { NumberAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic } }
}
