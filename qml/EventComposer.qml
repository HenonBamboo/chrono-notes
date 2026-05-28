import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: composer

    property int stage: 0
    property color inkColor: "#071426"
    property alias inputActiveFocus: input.activeFocus

    signal addRequested(string text)
    signal emptySubmitted()

    height: 42
    spacing: 10

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
        placeholderText: composer.stage === 0 ? "输入新事件，按 Enter 添加" : "输入本阶段计划，按 Enter 添加"
        font.pixelSize: 14
        font.family: "Microsoft YaHei UI"
        color: composer.inkColor
        selectionColor: "#f4d676"
        selectedTextColor: composer.inkColor
        renderType: Text.NativeRendering
        background: Rectangle {
            radius: 15
            color: "#ccfffef7"
            border.color: input.activeFocus ? "#662d68c7" : "#226a5e2a"
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
