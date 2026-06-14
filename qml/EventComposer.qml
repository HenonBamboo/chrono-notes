import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: composer

    readonly property ChronoTokens tokens: ChronoTokens {}

    property int stage: 0
    property color inkColor: tokens.ink
    property alias inputActiveFocus: input.activeFocus

    signal addRequested(string text)
    signal emptySubmitted()

    height: 40
    spacing: tokens.space2

    function forceComposerFocus() {
        input.forceActiveFocus()
        input.cursorPosition = input.length
    }

    function releaseComposerFocus() {
        input.focus = false
    }

    TextField {
        id: input
        Layout.fillWidth: true
        Layout.fillHeight: true
        placeholderText: composer.stage === 0 ? "输入新便签，按 Enter 添加" : "输入本阶段便签，按 Enter 添加"
        font.pixelSize: 14
        font.family: tokens.fontUi
        color: composer.inkColor
        selectionColor: tokens.accentYellowSoft
        selectedTextColor: composer.inkColor
        renderType: Text.NativeRendering
        background: Rectangle {
            radius: tokens.radiusMd
            color: "#ddfffef7"
            border.color: input.activeFocus ? "#662d68c7" : tokens.lineSoft
            border.width: 1
            Behavior on border.color { ColorAnimation { duration: 150 } }
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
