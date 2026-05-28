import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool active: false
    property real widthHint: 72
    property bool primary: false
    property bool danger: false

    implicitWidth: widthHint
    implicitHeight: 34
    hoverEnabled: true
    clip: true

    contentItem: Text {
        width: control.width
        height: control.height
        text: control.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: control.primary || control.danger ? "#ffffff" : "#071426"
        font.pixelSize: 12
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 10
        font.weight: Font.DemiBold
        font.family: "Microsoft YaHei UI"
        elide: Text.ElideRight
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        id: bg
        radius: height / 2
        color: control.primary
            ? (control.active ? "#14345c" : "#2d68c7")
            : control.danger ? (control.hovered ? "#a32929" : "#c93636")
            : control.active ? "#f4bf30" : control.hovered ? "#fff9d9" : "#fffef7"
        border.width: control.primary || control.danger ? 0 : 1
        border.color: control.active ? "#d4a51f" : "#d8c77d"

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 8
            height: parent.height + 8
            radius: height / 2
            color: control.primary ? "#77ffffff" : "#55f4bf30"
            opacity: control.pressed ? 0.65 : 0
            scale: control.pressed ? 1 : 0.82
            Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        }

        Behavior on color { ColorAnimation { duration: 160 } }
    }

    scale: control.pressed ? 0.96 : control.hovered ? 1.015 : 1
    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
}
