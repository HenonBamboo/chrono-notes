import QtQuick
import QtQuick.Controls

Button {
    id: control

    readonly property ChronoTokens tokens: ChronoTokens {}

    property bool active: false
    property real widthHint: 72
    property bool primary: false
    property bool danger: false

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
        color: !control.enabled ? tokens.mutedSoft
              : control.danger ? tokens.danger
              : control.primary ? "#1f56aa"
              : control.active ? tokens.ink
              : tokens.ink
        font.pixelSize: tokens.sizeBody
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 10
        font.weight: Font.DemiBold
        font.family: tokens.fontUi
        elide: Text.ElideRight
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        id: bg
        radius: height / 2
        color: !control.enabled ? "#55fffef7"
            : control.primary ? (control.hovered ? "#f4f8ff" : tokens.accentBlueSoft)
            : control.danger ? (control.hovered ? tokens.dangerSoft : "#00ffffff")
            : control.active ? tokens.accentYellowSoft
            : control.hovered ? tokens.card : "#bbfffef7"
        border.width: 1
        border.color: !control.enabled ? tokens.lineSoft
                    : control.primary ? "#572d68c7"
                    : control.danger ? "#44b4534a"
                    : control.active ? "#77f4bf30"
                    : control.visualFocus ? tokens.accentBlue
                    : tokens.line

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 8
            height: parent.height + 8
            radius: height / 2
            color: control.primary ? "#552d68c7" : "#55f4bf30"
            opacity: control.pressed ? 0.36 : 0
            scale: control.pressed ? 1 : 0.86
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }

        Behavior on color { ColorAnimation { duration: 140 } }
        Behavior on border.color { ColorAnimation { duration: 140 } }
    }

    y: control.pressed ? 1 : 0
    scale: control.hovered && control.enabled ? 1.01 : 1
    Behavior on y { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
}
