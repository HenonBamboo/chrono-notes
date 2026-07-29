import QtQuick

Text {
    id: meta

    readonly property ChronoTokens fallbackTokens: ChronoTokens {}
    readonly property var tokens: meta.theme ? meta.theme : meta.fallbackTokens

    property var theme: null
    property string uiFontFamily: tokens.fontUi
    property int uiFontSize: tokens.sizeMeta
    property color textColor: tokens.textSecondary

    color: textColor
    font.pixelSize: uiFontSize
    font.family: uiFontFamily
    renderType: Text.NativeRendering
}
