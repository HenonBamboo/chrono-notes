import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string label: ""
    property bool danger: false

    width: 32
    height: 28
    hoverEnabled: true

    contentItem: Text {
        text: control.label
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: control.danger ? "#a8322f" : "#53647f"
        font.pixelSize: 14
        font.family: "Microsoft YaHei UI"
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        radius: 10
        color: control.hovered ? (control.danger ? "#ffe7e7" : "#fff5c6") : "transparent"
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
