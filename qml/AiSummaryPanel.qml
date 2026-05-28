pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property bool aiBusy: false
    property string aiResultText: ""
    property alias inputActiveFocus: requirement.activeFocus

    signal runRequested(string requirement)

    spacing: 13

    function releaseInputFocus() {
        requirement.focus = false
    }

    Text {
        text: "智能摘要"
        color: "#071426"
        font.pixelSize: 23
        font.weight: Font.Bold
        font.family: "Microsoft YaHei UI"
        renderType: Text.NativeRendering
    }

    Text {
        Layout.fillWidth: true
        text: "从右侧挤出，主内容同步让位；再次点击智能摘要即可收起。"
        color: "#607086"
        font.pixelSize: 13
        font.family: "Microsoft YaHei UI"
        wrapMode: Text.WordWrap
        renderType: Text.NativeRendering
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 134
        radius: 18
        color: "#99ffffff"
        border.color: "transparent"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            Text {
                text: "总结要求"
                color: "#172033"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.family: "Microsoft YaHei UI"
                renderType: Text.NativeRendering
            }

            TextArea {
                id: requirement
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: "请总结当前阶段的完成情况，指出未完成事项的优先级，并给出下一步建议。"
                wrapMode: TextEdit.WrapAnywhere
                font.pixelSize: 13
                font.family: "Microsoft YaHei UI"
                color: "#172033"
                renderType: Text.NativeRendering
                background: Rectangle {
                    radius: 14
                    color: "#b8ffffff"
                    border.color: requirement.activeFocus ? "#2d68c7" : "transparent"
                    border.width: 1
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Repeater {
            model: [
                { label: "快速", prompt: "请用三句话总结当前阶段的完成情况，并列出最重要的未完成事项。" },
                { label: "复盘", prompt: "请总结完成情况、卡点原因，并给出下一步优先级建议。" },
                { label: "计划", prompt: "请把未完成事项整理成下一阶段的清晰计划，按优先级排序。" }
            ]
            delegate: ToolPill {
                required property var modelData
                text: modelData.label
                widthHint: 58
                onClicked: requirement.text = modelData.prompt
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 88
        radius: 18
        color: "#99ffffff"
        border.color: "transparent"

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 7

            Text {
                text: "阶段判断"
                color: "#172033"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.family: "Microsoft YaHei UI"
                renderType: Text.NativeRendering
            }

            Text {
                width: parent.width
                text: "摘要会结合当前时间视图、计划内容和自动收纳内容生成。"
                color: "#5f6f86"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                font.family: "Microsoft YaHei UI"
                renderType: Text.NativeRendering
            }
        }
    }

    Button {
        id: runAi
        Layout.fillWidth: true
        Layout.preferredHeight: 44
        enabled: !root.aiBusy
        hoverEnabled: true
        clip: true
        onClicked: root.runRequested(requirement.text)
        contentItem: Text {
            text: root.aiBusy ? "生成中..." : "生成摘要"
            color: "#ffffff"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 14
            font.weight: Font.Bold
            font.family: "Microsoft YaHei UI"
            renderType: Text.NativeRendering
        }
        background: Rectangle {
            id: runBg
            radius: 16
            color: runAi.hovered ? "#245db6" : "#2d68c7"

            Rectangle {
                width: parent.width * 0.36
                height: parent.height + 8
                y: -4
                opacity: 0.45
                rotation: 12
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#00ffffff" }
                    GradientStop { position: 0.5; color: "#ccffffff" }
                    GradientStop { position: 1.0; color: "#00ffffff" }
                }
                SequentialAnimation on x {
                    running: root.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: -runBg.width * 0.5; to: runBg.width + 20; duration: 1800; easing.type: Easing.InOutCubic }
                    PauseAnimation { duration: 1000 }
                }
            }
        }
    }

    Text {
        text: "结果"
        color: "#607086"
        font.pixelSize: 12
        font.family: "Microsoft YaHei UI"
        renderType: Text.NativeRendering
    }

    TextArea {
        Layout.fillWidth: true
        Layout.fillHeight: true
        readOnly: true
        text: root.aiResultText
        wrapMode: TextEdit.WrapAnywhere
        placeholderText: "生成后的摘要会出现在这里。"
        font.pixelSize: 13
        font.family: "Microsoft YaHei UI"
        color: "#172033"
        renderType: Text.NativeRendering
        background: Rectangle {
            radius: 18
            color: "#99ffffff"
            border.color: "transparent"
            border.width: 0
        }
    }
}
