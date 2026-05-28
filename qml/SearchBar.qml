pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: search

    property bool open: false
    property string query: ""
    property int completionFilter: -1
    property color inkColor: "#071426"
    property color mutedColor: "#64748b"

    signal queryEdited(string text)
    signal completionFilterRequested(int value)
    signal closeRequested()

    function forceSearchFocus() {
        field.forceActiveFocus()
        field.selectAll()
    }

    height: open || query.length > 0 ? 74 : 0
    opacity: open || query.length > 0 ? 1 : 0
    clip: true

    Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 120 } }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: "#bffffdf2"
        border.width: field.activeFocus ? 1 : 0
        border.color: "#55326dcc"

        TextField {
            id: field
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 38
            anchors.leftMargin: 14
            anchors.rightMargin: clearButton.width + 10
            text: search.query
            placeholderText: "搜索全部便签，按 Esc 关闭"
            font.pixelSize: 13
            font.family: "Microsoft YaHei UI"
            color: search.inkColor
            placeholderTextColor: "#8a64748b"
            selectionColor: "#f4d676"
            selectedTextColor: search.inkColor
            renderType: Text.NativeRendering
            background: Item {}

            onTextEdited: search.queryEdited(text)

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape) {
                    text = ""
                    search.queryEdited("")
                    search.closeRequested()
                    event.accepted = true
                }
            }
        }

        Button {
            id: clearButton
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.top: parent.top
            anchors.topMargin: 7
            width: 24
            height: 24
            visible: field.text.length > 0
            text: "×"
            hoverEnabled: true
            onClicked: {
                field.text = ""
                search.queryEdited("")
                search.closeRequested()
            }

            contentItem: Text {
                text: clearButton.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: clearButton.hovered ? "#c43131" : search.mutedColor
                font.pixelSize: 15
                font.family: "Microsoft YaHei UI"
                renderType: Text.NativeRendering
            }

            background: Rectangle {
                radius: width / 2
                color: clearButton.hovered ? "#1ac43131" : "transparent"
                Behavior on color { ColorAnimation { duration: 120 } }
            }
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.top: field.bottom
            height: 28
            spacing: 8

            ToolPill {
                text: "全部"
                widthHint: 56
                active: search.completionFilter < 0
                onClicked: search.completionFilterRequested(-1)
            }

            ToolPill {
                text: "未完成"
                widthHint: 64
                active: search.completionFilter === 0
                onClicked: search.completionFilterRequested(0)
            }

            ToolPill {
                text: "已完成"
                widthHint: 64
                active: search.completionFilter === 1
                onClicked: search.completionFilterRequested(1)
            }
        }
    }
}
