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
    property var theme: null
    property string uiFontFamily: tokens.fontUi
    property int uiFontSize: tokens.sizeBody

    implicitWidth: widthHint
    implicitHeight: primary ? 32 : 30
    hoverEnabled: true
    clip: true

    contentItem: Text {
        width: control.width
        height: control.height
        text: control.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: !control.enabled ? control.tokens.mutedSoft
              : control.danger ? control.tokens.danger
              : control.primary ? "#1f56aa"
              : control.active ? control.tokens.ink
              : control.tokens.ink
        font.pixelSize: control.uiFontSize
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 10
        font.weight: Font.DemiBold
        font.family: control.uiFontFamily
        elide: Text.ElideRight
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        id: bg
        radius: height / 2
        color: !control.enabled ? "#55fffef7"
            : control.primary ? (control.hovered ? "#f4f8ff" : control.tokens.accentBlueSoft)
            : control.danger ? (control.hovered ? control.tokens.dangerSoft : "#00ffffff")
            : control.active ? "#f4fbef"
            : control.hovered ? "#f8fff7" : "#bbfffef7"
        border.width: 1
        border.color: !control.enabled ? control.tokens.lineSoft
                    : control.primary ? "#572d68c7"
                    : control.danger ? "#44b4534a"
                    : control.active ? "#6688c57f"
                    : control.visualFocus ? control.tokens.accentBlue
                    : control.tokens.line

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: height / 2
            color: control.tokens.lightGlow
            opacity: control.hovered && control.enabled ? 0.42 : control.active ? 0.22 : 0
            Behavior on opacity { NumberAnimation { duration: control.tokens.motionMedium; easing.type: Easing.OutCubic } }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 8
            height: parent.height + 8
            radius: height / 2
            color: control.primary ? "#552d68c7" : control.tokens.activeGlow
            opacity: control.pressed ? 0.36 : 0
            scale: control.pressed ? 1 : 0.86
            Behavior on opacity { NumberAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic } }
        }

        Behavior on color { ColorAnimation { duration: control.tokens.motionMedium; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: control.tokens.motionMedium; easing.type: Easing.OutCubic } }
    }

    y: control.pressed ? 1 : 0
    scale: control.pressed ? 0.985 : (control.hovered && control.enabled ? 1.01 : 1)
    Behavior on y { NumberAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic } }
}
