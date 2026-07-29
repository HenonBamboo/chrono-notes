import QtQuick
import QtQuick.Controls

Button {
    id: control

    readonly property ChronoTokens fallbackTokens: ChronoTokens {}
    readonly property var tokens: control.theme ? control.theme : control.fallbackTokens

    property bool active: false
    property real widthHint: 72
    property bool primary: false
    property bool danger: false
    property bool projectStyle: false
    property string accessibleDescription: active ? "当前已选择" : danger ? "执行危险操作" : "按下以执行"
    readonly property int focusBorderWidth: control.visualFocus ? 2 : 1
    property var theme: null
    property string uiFontFamily: tokens.fontUi
    property int uiFontSize: tokens.sizeBody

    implicitWidth: Math.max(widthHint, tokens.controlHeight)
    implicitHeight: tokens.controlHeight
    hoverEnabled: true
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: accessibleDescription

    contentItem: Text {
        text: control.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: !control.enabled ? control.tokens.textDisabled
              : control.primary ? control.tokens.textOnAccent
              : control.danger ? control.tokens.danger
              : control.active ? (control.projectStyle ? control.tokens.projectAccent : control.tokens.accent)
              : control.tokens.ink
        font.pixelSize: control.uiFontSize
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 12
        font.weight: control.active || control.primary ? Font.DemiBold : Font.Medium
        font.family: control.uiFontFamily
        elide: Text.ElideRight
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        radius: control.tokens.radiusPill
        color: !control.enabled ? control.tokens.surfaceDisabled
             : control.primary ? (control.pressed ? control.tokens.accentPressed
                                  : control.hovered ? control.tokens.accentHover
                                                    : control.tokens.accent)
             : control.danger ? (control.hovered || control.pressed
                                 ? control.tokens.dangerSoft : control.tokens.transparent)
             : control.active ? (control.projectStyle
                                 ? control.tokens.projectAccentSoft : control.tokens.accentSoft)
             : control.pressed ? control.tokens.surfacePressed
             : control.hovered ? control.tokens.surfaceHover
                               : control.tokens.surface
        border.width: control.focusBorderWidth
        border.color: control.visualFocus ? control.tokens.focusRing
                    : !control.enabled ? control.tokens.borderSubtle
                    : control.primary ? control.tokens.accent
                    : control.danger ? control.tokens.danger
                    : control.active ? (control.projectStyle
                                        ? control.tokens.projectAccent : control.tokens.accent)
                    : control.tokens.border

        Behavior on color {
            ColorAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic }
        }
    }

    y: control.pressed ? 1 : 0
    scale: control.pressed ? 0.985 : 1
    Behavior on y {
        NumberAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic }
    }
    Behavior on scale {
        NumberAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic }
    }
}
