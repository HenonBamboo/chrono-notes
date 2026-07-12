pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: tabs

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: tabs.uiFontFamily
        baseFontSize: tabs.uiFontSize
    }
    readonly property var tokens: tabs.theme ? tabs.theme : tabs.fallbackTokens

    property int stage: 0
    property bool allCompleted: false
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    readonly property real contentHeight: tabFlow.implicitHeight

    signal stageRequested(int stage)
    signal toggleAllRequested()

    height: contentHeight

    function stageName(index) {
        return ["每天", "每周", "每月", "每年"][index]
    }

    Flow {
        id: tabFlow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: tabs.tokens.space2

        Repeater {
            model: 4
            delegate: ToolPill {
                required property int index
                text: tabs.stageName(index)
                active: tabs.stage === index
                widthHint: 50
                theme: tabs.tokens
                uiFontFamily: tabs.uiFontFamily
                uiFontSize: tabs.uiFontSize
                onClicked: tabs.stageRequested(index)
            }
        }

        ToolPill {
            text: tabs.allCompleted ? "取消全选" : "全选完成"
            widthHint: 76
            theme: tabs.tokens
            uiFontFamily: tabs.uiFontFamily
            uiFontSize: tabs.uiFontSize
            onClicked: tabs.toggleAllRequested()
        }
    }
}
