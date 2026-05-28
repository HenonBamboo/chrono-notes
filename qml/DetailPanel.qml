pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string eventText: ""
    property string eventMeta: ""
    property string eventRepeat: ""
    property bool readOnly: false
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
        text: root.readOnly ? "收纳详情" : "事件详情"
        color: "#071426"
        font.pixelSize: 23
        font.weight: Font.Bold
        font.family: "Microsoft YaHei UI"
        renderType: Text.NativeRendering
    }

    Text {
        Layout.fillWidth: true
        text: root.eventMeta
        color: "#607086"
        font.pixelSize: 13
        font.family: "Microsoft YaHei UI"
        wrapMode: Text.WordWrap
        renderType: Text.NativeRendering
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 18
        color: "#99ffffff"
        border.width: 0

        TextArea {
            id: detailEditor
            anchors.fill: parent
            anchors.margins: 14
            text: root.eventText
            readOnly: root.readOnly
            wrapMode: TextEdit.WrapAnywhere
            selectByMouse: true
            font.pixelSize: 14
            font.family: "Microsoft YaHei UI"
            color: root.readOnly ? "#46566f" : "#172033"
            selectedTextColor: "#071426"
            selectionColor: "#f4d676"
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
        color: "#7a879b"
        font.pixelSize: 12
        font.family: "Microsoft YaHei UI"
        renderType: Text.NativeRendering
    }

    RowLayout {
        Layout.fillWidth: true
        visible: !root.readOnly
        spacing: 8

        Text {
            text: "重复"
            color: "#607086"
            font.pixelSize: 12
            font.family: "Microsoft YaHei UI"
            renderType: Text.NativeRendering
        }

        ToolPill {
            text: "无"
            widthHint: 44
            active: root.eventRepeat.length === 0
            onClicked: root.repeatRequested("")
        }

        ToolPill {
            text: "每天"
            widthHint: 52
            active: root.eventRepeat === "daily"
            onClicked: root.repeatRequested("daily")
        }

        ToolPill {
            text: "每周"
            widthHint: 52
            active: root.eventRepeat === "weekly"
            onClicked: root.repeatRequested("weekly")
        }

        ToolPill {
            text: "每月"
            widthHint: 52
            active: root.eventRepeat === "monthly"
            onClicked: root.repeatRequested("monthly")
        }

        ToolPill {
            text: "每年"
            widthHint: 52
            active: root.eventRepeat === "yearly"
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
            font.pixelSize: 14
            font.weight: Font.Bold
            font.family: "Microsoft YaHei UI"
            renderType: Text.NativeRendering
        }

        background: Rectangle {
            radius: 16
            color: saveButton.hovered ? "#245db6" : "#2d68c7"
            Behavior on color { ColorAnimation { duration: 130 } }
        }
    }
}
