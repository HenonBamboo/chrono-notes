pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: root.uiFontFamily
        baseFontSize: root.uiFontSize
    }
    readonly property var tokens: root.theme ? root.theme : root.fallbackTokens

    property string eventText: ""
    property string eventMeta: ""
    property string eventRepeat: ""
    property bool readOnly: false
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    property alias inputActiveFocus: detailEditor.activeFocus

    signal saveRequested(string text)
    signal repeatRequested(string repeat)

    spacing: root.tokens.space3

    function releaseInputFocus() {
        detailEditor.focus = false
    }

    function focusInitial() {
        detailEditor.forceActiveFocus(Qt.TabFocusReason)
    }

    onEventTextChanged: {
        if (detailEditor.text !== root.eventText)
            detailEditor.text = root.eventText
    }

    Text {
        text: root.readOnly ? "收纳详情" : "便签详情"
        color: root.tokens.ink
        font.pixelSize: root.tokens.sizeDisplay
        font.weight: Font.Bold
        font.family: root.tokens.fontUi
        renderType: Text.NativeRendering
    }

    Text {
        Layout.fillWidth: true
        text: root.eventMeta
        color: root.tokens.muted
        font.pixelSize: root.tokens.sizeMeta
        font.family: root.tokens.fontUi
        wrapMode: Text.WordWrap
        renderType: Text.NativeRendering
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: root.tokens.radiusLg
        color: detailEditor.activeFocus ? root.tokens.surfaceHover : root.tokens.surface
        border.width: detailEditor.activeFocus ? 2 : 1
        border.color: detailEditor.activeFocus ? root.tokens.focusRing : root.tokens.border

        Behavior on color { ColorAnimation { duration: root.tokens.motionMedium; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: root.tokens.motionMedium; easing.type: Easing.OutCubic } }

        TextArea {
            id: detailEditor
            objectName: "detailEditor"
            anchors.fill: parent
            anchors.margins: 14
            text: root.eventText
            readOnly: root.readOnly
            wrapMode: TextEdit.WrapAnywhere
            selectByMouse: true
            font.pixelSize: root.tokens.sizeBody
            font.family: root.tokens.fontUi
            color: root.readOnly ? root.tokens.textSecondary : root.tokens.ink
            selectedTextColor: root.tokens.ink
            selectionColor: root.tokens.accentSoft
            activeFocusOnTab: true
            renderType: Text.NativeRendering
            background: Item {}

            Accessible.role: Accessible.EditableText
            Accessible.name: root.readOnly ? "便签详情" : "编辑便签详情"
            Accessible.description: root.readOnly
                                    ? "只读便签内容"
                                    : "编辑便签内容，按 Ctrl+Enter 保存"

            Keys.onPressed: function(event) {
                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) &&
                        (event.modifiers & Qt.ControlModifier) && !root.readOnly) {
                    root.saveRequested(text)
                    event.accepted = true
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: !root.readOnly
        text: "可直接编辑，按 Ctrl+Enter 保存。"
        color: root.tokens.mutedSoft
        font.pixelSize: root.tokens.sizeMeta
        font.family: root.tokens.fontUi
        renderType: Text.NativeRendering
    }

    RowLayout {
        Layout.fillWidth: true
        visible: !root.readOnly
        spacing: 8

        Text {
            text: "重复"
            color: root.tokens.muted
            font.pixelSize: root.tokens.sizeBody
            font.family: root.tokens.fontUi
            renderType: Text.NativeRendering
        }

        ToolPill {
            text: "无"
            widthHint: 44
            active: root.eventRepeat.length === 0
            theme: root.tokens
            uiFontFamily: root.uiFontFamily
            uiFontSize: root.uiFontSize
            accessibleDescription: "关闭重复计划"
            onClicked: root.repeatRequested("")
        }

        ToolPill {
            text: "每天"
            widthHint: 52
            active: root.eventRepeat === "daily"
            theme: root.tokens
            uiFontFamily: root.uiFontFamily
            uiFontSize: root.uiFontSize
            accessibleDescription: "设置为每天重复"
            onClicked: root.repeatRequested("daily")
        }

        ToolPill {
            text: "每周"
            widthHint: 52
            active: root.eventRepeat === "weekly"
            theme: root.tokens
            uiFontFamily: root.uiFontFamily
            uiFontSize: root.uiFontSize
            accessibleDescription: "设置为每周重复"
            onClicked: root.repeatRequested("weekly")
        }

        ToolPill {
            text: "每月"
            widthHint: 52
            active: root.eventRepeat === "monthly"
            theme: root.tokens
            uiFontFamily: root.uiFontFamily
            uiFontSize: root.uiFontSize
            accessibleDescription: "设置为每月重复"
            onClicked: root.repeatRequested("monthly")
        }

        ToolPill {
            text: "每年"
            widthHint: 52
            active: root.eventRepeat === "yearly"
            theme: root.tokens
            uiFontFamily: root.uiFontFamily
            uiFontSize: root.uiFontSize
            accessibleDescription: "设置为每年重复"
            onClicked: root.repeatRequested("yearly")
        }
    }

    Button {
        id: saveButton
        objectName: "detailSaveButton"
        visible: !root.readOnly
        Layout.fillWidth: true
        Layout.preferredHeight: root.tokens.primaryControlHeight
        hoverEnabled: true
        activeFocusOnTab: true
        onClicked: root.saveRequested(detailEditor.text)

        Accessible.role: Accessible.Button
        Accessible.name: "保存详情"
        Accessible.description: "保存当前便签内容"

        contentItem: Text {
            text: "保存详情"
            color: root.tokens.textOnAccent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: root.tokens.sizeBody
            font.weight: Font.Bold
            font.family: root.tokens.fontUi
            renderType: Text.NativeRendering
        }

        background: Rectangle {
            radius: root.tokens.radiusMd
            color: saveButton.pressed ? root.tokens.accentPressed
                 : saveButton.hovered ? root.tokens.accentHover : root.tokens.accent
            border.width: saveButton.visualFocus ? 2 : 0
            border.color: root.tokens.focusRing
            Behavior on color { ColorAnimation { duration: root.tokens.motionFast; easing.type: Easing.OutCubic } }
        }

        scale: saveButton.pressed ? 0.99 : saveButton.hovered ? 1.01 : 1
        Behavior on scale { NumberAnimation { duration: root.tokens.motionFast; easing.type: Easing.OutCubic } }
    }
}
