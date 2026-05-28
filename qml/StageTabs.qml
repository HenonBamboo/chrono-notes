pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: tabs

    property int stage: 0
    property bool allCompleted: false
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
        spacing: 8

        Repeater {
            model: 4
            delegate: ToolPill {
                required property int index
                text: tabs.stageName(index)
                active: tabs.stage === index
                widthHint: 48
                onClicked: tabs.stageRequested(index)
            }
        }

        ToolPill {
            text: tabs.allCompleted ? "取消全选" : "全选完成"
            widthHint: 70
            onClicked: tabs.toggleAllRequested()
        }
    }
}
