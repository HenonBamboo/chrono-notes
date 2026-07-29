import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: composer

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: composer.uiFontFamily
        baseFontSize: composer.uiFontSize
    }
    readonly property var tokens: composer.theme ? composer.theme : composer.fallbackTokens

    property int stage: 0
    property color inkColor: composer.tokens.ink
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    property alias inputActiveFocus: input.activeFocus

    signal addRequested(string text)
    signal emptySubmitted()

    height: 40
    spacing: composer.tokens.space2

    function forceComposerFocus() {
        input.forceActiveFocus()
        input.cursorPosition = input.length
    }

    function releaseComposerFocus() {
        input.focus = false
    }

    TextField {
        id: input
        objectName: "composerInput"
        Layout.fillWidth: true
        Layout.fillHeight: true
        placeholderText: composer.stage === 0 ? "输入新便签，按 Enter 添加" : "输入本阶段便签，按 Enter 添加"
        font.pixelSize: composer.tokens.sizeBody + 2
        font.family: composer.tokens.fontUi
        color: composer.inkColor
        selectionColor: composer.tokens.accentSoft
        selectedTextColor: composer.inkColor
        maximumLength: 65536
        activeFocusOnTab: true
        renderType: Text.NativeRendering

        Accessible.role: Accessible.EditableText
        Accessible.name: "新建便签"
        Accessible.description: "输入便签内容，按 Enter 添加到当前阶段"

        background: Rectangle {
            radius: composer.tokens.radiusMd
            color: composer.tokens.surface
            border.color: input.activeFocus ? composer.tokens.focusRing : composer.tokens.border
            border.width: input.activeFocus ? 2 : 1
            Behavior on border.color { ColorAnimation { duration: composer.tokens.motionFast; easing.type: Easing.OutCubic } }
        }
        onAccepted: {
            const trimmed = text.trim()
            if (trimmed.length === 0) {
                composer.emptySubmitted()
                return
            }
            composer.addRequested(trimmed)
            text = ""
        }
    }
}
