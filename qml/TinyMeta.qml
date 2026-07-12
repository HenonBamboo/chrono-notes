import QtQuick

Text {
    property string uiFontFamily: "Microsoft YaHei UI"
    property int uiFontSize: 12

    color: "#68758f"
    font.pixelSize: uiFontSize
    font.family: uiFontFamily
    renderType: Text.NativeRendering
}
