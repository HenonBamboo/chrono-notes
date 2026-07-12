pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: surface
    objectName: "aiPanelSurface"

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: surface.uiFontFamily
        baseFontSize: surface.uiFontSize
    }
    readonly property var tokens: surface.theme ? surface.theme : surface.fallbackTokens

    property bool aiBusy: false
    property string aiResultText: ""
    property string summaryScope: "stickies"
    property string summaryContextText: ""
    property bool summaryEmpty: false
    property color surfaceColor: tokens.drawerPaper
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    readonly property bool projectScope: summaryScope === "projects"
    property alias inputActiveFocus: requirement.activeFocus

    signal runRequested(string requirement, string contextText)

    color: surface.surfaceColor
    radius: tokens.radiusLg
    border.width: 0

    function defaultPrompt() {
        if (projectScope)
            return "请总结当前项目树的推进情况，指出未完成任务、层级风险和下一步优先级。"
        return "请总结当前便签视图的完成情况，指出未完成事项的优先级，并给出下一步建议。"
    }

    function releaseInputFocus() {
        requirement.focus = false
    }

    function runSummary() {
        if (aiBusy || summaryEmpty)
            return
        runRequested(requirement.text, projectScope ? summaryContextText : "")
    }

    onProjectScopeChanged: requirement.text = defaultPrompt()

    ColumnLayout {
        anchors.fill: parent
        spacing: surface.tokens.space3

        Text {
            objectName: "aiPanelTitle"
            text: surface.projectScope ? "项目摘要" : "便签摘要"
            color: surface.tokens.ink
            font.pixelSize: surface.tokens.sizeTitle + 7
            font.weight: Font.Bold
            font.family: surface.tokens.fontUi
            renderType: Text.NativeRendering
        }

        Text {
            objectName: "aiPanelDescription"
            Layout.fillWidth: true
            text: surface.projectScope
                  ? "根据当前项目树的选中节点、路径、进度、具体内容和任务数量生成摘要。"
                  : "根据当前便签视图、阶段、搜索结果和自动收纳内容生成摘要。"
            color: surface.tokens.muted
            font.pixelSize: surface.tokens.sizeBody + 1
            font.family: surface.tokens.fontUi
            wrapMode: Text.WordWrap
            renderType: Text.NativeRendering
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 132
            radius: surface.tokens.radiusLg
            color: surface.tokens.drawerCard
            border.color: surface.tokens.lineSoft

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: surface.tokens.space3
                spacing: surface.tokens.space2

                Text {
                    text: "摘要要求"
                    color: surface.tokens.ink
                    font.pixelSize: surface.tokens.sizeBody + 1
                    font.weight: Font.DemiBold
                    font.family: surface.tokens.fontUi
                    renderType: Text.NativeRendering
                }

                TextArea {
                    id: requirement
                    objectName: "aiRequirementInput"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: surface.defaultPrompt()
                    wrapMode: TextEdit.WrapAnywhere
                    font.pixelSize: surface.tokens.sizeBody + 1
                    font.family: surface.tokens.fontUi
                    color: surface.tokens.ink
                    renderType: Text.NativeRendering
                    background: Rectangle {
                        radius: surface.tokens.radiusMd
                        color: "#b8ffffff"
                        border.color: requirement.activeFocus ? surface.tokens.accentBlue : "transparent"
                        border.width: 1
                        Behavior on border.color { ColorAnimation { duration: surface.tokens.motionFast; easing.type: Easing.OutCubic } }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: surface.tokens.space2

            Repeater {
                model: surface.projectScope ? [
                    { label: "概况", prompt: "请用三句话总结项目树概况，并列出最需要推进的未完成任务。" },
                    { label: "风险", prompt: "请找出当前项目或任务的主要卡点、缺口和下一步处理顺序。" },
                    { label: "计划", prompt: "请把项目树整理成下一轮行动计划，按优先级排序。" }
                ] : [
                    { label: "快速", prompt: "请用三句话总结当前便签视图，并列出最重要的未完成事项。" },
                    { label: "复盘", prompt: "请总结便签完成情况、卡点原因，并给出下一步优先级建议。" },
                    { label: "计划", prompt: "请把未完成便签整理成下一阶段清晰计划，按优先级排序。" }
                ]
                delegate: ToolPill {
                    required property var modelData
                    text: modelData.label
                    widthHint: 58
                    theme: surface.tokens
                    uiFontFamily: surface.uiFontFamily
                    uiFontSize: surface.uiFontSize
                    onClicked: requirement.text = modelData.prompt
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            radius: surface.tokens.radiusLg
            color: surface.tokens.drawerCard
            border.color: surface.tokens.lineSoft

            Column {
                anchors.fill: parent
                anchors.margins: surface.tokens.space3
                spacing: surface.tokens.space2

                Text {
                    text: surface.projectScope ? "项目上下文" : "便签上下文"
                    color: surface.tokens.ink
                    font.pixelSize: surface.tokens.sizeBody + 2
                    font.weight: Font.DemiBold
                    font.family: surface.tokens.fontUi
                    renderType: Text.NativeRendering
                }

                Text {
                    objectName: "aiScopeHint"
                    width: parent.width
                    text: surface.projectScope
                          ? (surface.summaryEmpty ? "先创建项目，再生成项目摘要。" : "会使用当前选中节点；未选中时总结整个项目树概况。")
                          : "会结合当前阶段、搜索结果和自动收纳内容生成。"
                    color: surface.tokens.muted
                    font.pixelSize: surface.tokens.sizeBody + 1
                    wrapMode: Text.WordWrap
                    font.family: surface.tokens.fontUi
                    renderType: Text.NativeRendering
                }
            }
        }

        Button {
            id: runAi
            objectName: "aiRunButton"
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            enabled: !surface.aiBusy && !surface.summaryEmpty
            hoverEnabled: true
            clip: true
            onClicked: surface.runSummary()
            contentItem: Text {
                text: surface.aiBusy ? "生成中..." : "生成摘要"
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: surface.tokens.sizeBody + 2
                font.weight: Font.Bold
                font.family: surface.tokens.fontUi
                renderType: Text.NativeRendering
            }
            background: Rectangle {
                radius: surface.tokens.radiusMd
                color: !runAi.enabled ? surface.tokens.mutedSoft
                     : runAi.hovered ? "#245db6" : surface.tokens.accentBlue
                Behavior on color { ColorAnimation { duration: surface.tokens.motionFast; easing.type: Easing.OutCubic } }
            }
        }

        Text {
            text: "结果"
            color: surface.tokens.muted
            font.pixelSize: surface.tokens.sizeBody
            font.family: surface.tokens.fontUi
            renderType: Text.NativeRendering
        }

        TextArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: true
            text: surface.aiResultText
            wrapMode: TextEdit.WrapAnywhere
            placeholderText: "生成后的摘要会出现在这里。"
            font.pixelSize: surface.tokens.sizeBody + 1
            font.family: surface.tokens.fontUi
            color: surface.tokens.ink
            renderType: Text.NativeRendering
            background: Rectangle {
                radius: surface.tokens.radiusLg
                color: surface.tokens.drawerCard
                border.color: "transparent"
                border.width: 0
            }
        }
    }
}
