import QtQuick
import QtQuick.Controls

Button {
    id: control

    readonly property ChronoTokens fallbackTokens: ChronoTokens {}
    readonly property var tokens: control.theme ? control.theme : control.fallbackTokens

    property string label: ""
    property url iconSource
    property url hoverIconSource
    property string accessibleName: label
    property string accessibleDescription: "窗口操作"
    property bool danger: false
    property var theme: null
    property string uiFontFamily: control.tokens.fontUi
    property int uiFontSize: control.tokens.sizeBody

    implicitWidth: control.tokens.controlHeight
    implicitHeight: control.tokens.controlHeight
    hoverEnabled: true
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription

    contentItem: Item {
        Image {
            anchors.centerIn: parent
            width: 20
            height: 20
            source: control.hovered && control.hoverIconSource.toString().length > 0
                    ? control.hoverIconSource : control.iconSource
            visible: control.iconSource.toString().length > 0
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 40
            sourceSize.height: 40
            smooth: true
            mipmap: true
        }

        Text {
            anchors.fill: parent
            visible: control.iconSource.toString().length === 0
            text: control.label
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: control.danger && control.hovered ? control.tokens.textOnAccent
                  : control.danger ? control.tokens.danger
                  : control.tokens.textSecondary
            font.pixelSize: Math.max(16, control.uiFontSize + 2)
            font.family: control.uiFontFamily
            renderType: Text.NativeRendering
        }
    }

    background: Rectangle {
        radius: control.tokens.radiusSm
        color: control.danger && control.hovered ? control.tokens.danger
             : control.pressed ? control.tokens.surfacePressed
             : control.hovered ? control.tokens.surfaceHover
                               : control.tokens.transparent
        border.width: control.visualFocus ? 2 : 0
        border.color: control.tokens.focusRing
        Behavior on color {
            ColorAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic }
        }
    }

    scale: control.pressed ? 0.96 : 1
    Behavior on scale {
        NumberAnimation { duration: control.tokens.motionFast; easing.type: Easing.OutCubic }
    }
}
