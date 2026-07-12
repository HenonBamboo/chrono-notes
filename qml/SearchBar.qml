pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: search

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: search.uiFontFamily
        baseFontSize: search.uiFontSize
    }
    readonly property var tokens: search.theme ? search.theme : search.fallbackTokens

    property bool open: false
    property string query: ""
    property int completionFilter: -1
    property color inkColor: search.tokens.ink
    property color mutedColor: search.tokens.muted
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    property alias inputActiveFocus: field.activeFocus

    signal queryEdited(string text)
    signal completionFilterRequested(int value)
    signal closeRequested()

    onQueryChanged: {
        if (field.text !== query)
            field.text = query
    }

    function forceSearchFocus() {
        field.forceActiveFocus()
        field.selectAll()
    }

    function clearAndFocus() {
        field.text = ""
        search.queryEdited("")
        field.forceActiveFocus()
    }

    height: open || query.length > 0 ? 66 : 0
    opacity: open || query.length > 0 ? 1 : 0
    clip: true

    Behavior on height { NumberAnimation { duration: search.tokens.motionMedium; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: search.tokens.motionFast; easing.type: Easing.OutCubic } }

    Rectangle {
        anchors.fill: parent
        radius: search.tokens.radiusMd
        color: "#ccfffef7"
        border.width: 1
        border.color: field.activeFocus ? "#552d68c7" : search.tokens.lineSoft

        TextField {
            id: field
            objectName: "searchInput"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 34
            anchors.leftMargin: search.tokens.space3
            anchors.rightMargin: clearButton.width + 10
            placeholderText: "搜索全部便签，按 Esc 关闭"
            font.pixelSize: search.tokens.sizeBody + 1
            font.family: search.tokens.fontUi
            color: search.inkColor
            placeholderTextColor: search.tokens.mutedSoft
            selectionColor: search.tokens.accentYellowSoft
            selectedTextColor: search.inkColor
            renderType: Text.NativeRendering
            background: Item {}

            Component.onCompleted: text = search.query
            onTextEdited: search.queryEdited(text)

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape) {
                    if (text.length > 0) {
                        search.clearAndFocus()
                    } else {
                        search.closeRequested()
                    }
                    event.accepted = true
                }
            }
        }

        Button {
            id: clearButton
            anchors.right: parent.right
            anchors.rightMargin: search.tokens.space2
            anchors.top: parent.top
            anchors.topMargin: 5
            width: 24
            height: 24
            visible: field.text.length > 0
            text: "×"
            hoverEnabled: true
            onClicked: {
                search.clearAndFocus()
            }

            contentItem: Text {
                text: clearButton.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: clearButton.hovered ? search.tokens.danger : search.mutedColor
                font.pixelSize: search.tokens.sizeTitle + 1
                font.family: search.tokens.fontUi
                renderType: Text.NativeRendering
            }

            background: Rectangle {
                radius: width / 2
                color: clearButton.hovered ? search.tokens.dangerSoft : "transparent"
                Behavior on color { ColorAnimation { duration: search.tokens.motionFast; easing.type: Easing.OutCubic } }
            }
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: search.tokens.space2
            anchors.right: parent.right
            anchors.rightMargin: search.tokens.space2
            anchors.top: field.bottom
            height: 28
            spacing: search.tokens.space2

            ToolPill {
                text: "全部"
                widthHint: 56
                active: search.completionFilter < 0
                theme: search.tokens
                uiFontFamily: search.uiFontFamily
                uiFontSize: search.uiFontSize
                onClicked: search.completionFilterRequested(-1)
            }

            ToolPill {
                text: "未完成"
                widthHint: 64
                active: search.completionFilter === 0
                theme: search.tokens
                uiFontFamily: search.uiFontFamily
                uiFontSize: search.uiFontSize
                onClicked: search.completionFilterRequested(0)
            }

            ToolPill {
                text: "已完成"
                widthHint: 64
                active: search.completionFilter === 1
                theme: search.tokens
                uiFontFamily: search.uiFontFamily
                uiFontSize: search.uiFontSize
                onClicked: search.completionFilterRequested(1)
            }
        }
    }
}
