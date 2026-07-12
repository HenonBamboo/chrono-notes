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

    spacing: 12

    function releaseInputFocus() {
        detailEditor.focus = false
    }

    onEventTextChanged: {
        if (detailEditor.text !== root.eventText)
            detailEditor.text = root.eventText
    }

    Text {
        text: root.readOnly ? "收纳详情" : "便签详情"
        color: root.tokens.ink
        font.pixelSize: root.tokens.sizeTitle + 7
        font.weight: Font.Bold
        font.family: root.tokens.fontUi
        renderType: Text.NativeRendering
    }

    Text {
        Layout.fillWidth: true
        text: root.eventMeta
        color: root.tokens.muted
        font.pixelSize: root.tokens.sizeBody + 1
        font.family: root.tokens.fontUi
        wrapMode: Text.WordWrap
        renderType: Text.NativeRendering
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: root.tokens.radiusLg
        color: detailEditor.activeFocus ? "#f8fff7" : "#99ffffff"
        border.width: 1
        border.color: detailEditor.activeFocus ? root.tokens.accentMint : "#00ffffff"

        Behavior on color { ColorAnimation { duration: root.tokens.motionMedium; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: root.tokens.motionMedium; easing.type: Easing.OutCubic } }

        TextArea {
            id: detailEditor
            anchors.fill: parent
            anchors.margins: 14
            text: root.eventText
            readOnly: root.readOnly
            wrapMode: TextEdit.WrapAnywhere
            selectByMouse: true
            font.pixelSize: root.tokens.sizeBody + 2
            font.family: root.tokens.fontUi
            color: root.readOnly ? "#46566f" : "#172033"
            selectedTextColor: root.tokens.ink
            selectionColor: root.tokens.accentYellowSoft
            renderType: Text.NativeRendering
            background: Item {}

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
        font.pixelSize: root.tokens.sizeBody
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
            onClicked: root.repeatRequested("")
        }

        ToolPill {
            text: "每天"
            widthHint: 52
            active: root.eventRepeat === "daily"
            theme: root.tokens
            uiFontFamily: root.uiFontFamily
            uiFontSize: root.uiFontSize
            onClicked: root.repeatRequested("daily")
        }

        ToolPill {
            text: "每周"
            widthHint: 52
            active: root.eventRepeat === "weekly"
            theme: root.tokens
            uiFontFamily: root.uiFontFamily
            uiFontSize: root.uiFontSize
            onClicked: root.repeatRequested("weekly")
        }

        ToolPill {
            text: "每月"
            widthHint: 52
            active: root.eventRepeat === "monthly"
            theme: root.tokens
            uiFontFamily: root.uiFontFamily
            uiFontSize: root.uiFontSize
            onClicked: root.repeatRequested("monthly")
        }

        ToolPill {
            text: "每年"
            widthHint: 52
            active: root.eventRepeat === "yearly"
            theme: root.tokens
            uiFontFamily: root.uiFontFamily
            uiFontSize: root.uiFontSize
            onClicked: root.repeatRequested("yearly")
        }
    }

    Button {
        id: saveButton
        visible: !root.readOnly
        Layout.fillWidth: true
        Layout.preferredHeight: 42
        hoverEnabled: true
        onClicked: root.saveRequested(detailEditor.text)

        contentItem: Text {
            text: "保存详情"
            color: "#ffffff"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: root.tokens.sizeBody + 2
            font.weight: Font.Bold
            font.family: root.tokens.fontUi
            renderType: Text.NativeRendering
        }

        background: Rectangle {
            radius: root.tokens.radiusMd
            color: saveButton.pressed ? "#1f56aa" : saveButton.hovered ? "#245db6" : root.tokens.accentBlue
            Behavior on color { ColorAnimation { duration: root.tokens.motionFast; easing.type: Easing.OutCubic } }
        }

        scale: saveButton.pressed ? 0.99 : saveButton.hovered ? 1.01 : 1
        Behavior on scale { NumberAnimation { duration: root.tokens.motionFast; easing.type: Easing.OutCubic } }
    }
}
