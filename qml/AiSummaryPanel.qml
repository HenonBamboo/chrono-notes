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
    property string aiState: "idle"
    property string aiError: ""
    property bool hasApiKey: false
    property string aiResultText: ""
    property string summaryScope: "stickies"
    property string summaryContextText: ""
    property bool summaryEmpty: false
    property color surfaceColor: tokens.drawerPaper
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    readonly property bool projectScope: summaryScope === "projects"
    readonly property bool hasFailure: aiState === "error" || aiState === "failed" || aiError.length > 0
    readonly property string statusTitle: !hasApiKey ? "尚未配置 AI"
                                         : aiBusy ? "正在生成摘要"
                                         : hasFailure ? "生成失败"
                                         : aiState === "cancelled" ? "已取消生成"
                                         : ""
    readonly property string statusDescription: !hasApiKey
                                                ? "请先在设置中保存 API Key、HTTPS 地址和模型名称。"
                                                : aiBusy
                                                  ? "正在处理当前不可变快照；可以随时取消。"
                                                  : hasFailure
                                                    ? aiError
                                                    : aiState === "cancelled"
                                                      ? "本次请求已取消，不会覆盖已有结果。"
                                                      : ""
    property alias inputActiveFocus: requirement.activeFocus

    signal runRequested(string requirement, string contextText)
    signal cancelRequested()

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
        if (aiBusy || summaryEmpty || !hasApiKey)
            return
        runRequested(requirement.text, projectScope ? summaryContextText : "")
    }

    function focusInitial() {
        requirement.forceActiveFocus(Qt.TabFocusReason)
    }

    onProjectScopeChanged: requirement.text = defaultPrompt()

    ColumnLayout {
        anchors.fill: parent
        spacing: surface.tokens.space3

        Text {
            objectName: "aiPanelTitle"
            text: surface.projectScope ? "项目摘要" : "便签摘要"
            color: surface.tokens.ink
            font.pixelSize: surface.tokens.sizeDisplay
            font.weight: Font.Bold
            font.family: surface.tokens.fontUi
            renderType: Text.NativeRendering
        }

        Rectangle {
            objectName: "aiStatusPanel"
            Layout.fillWidth: true
            Layout.preferredHeight: statusColumn.implicitHeight + surface.tokens.space3 * 2
            visible: surface.statusTitle.length > 0
            radius: surface.tokens.radiusMd
            color: surface.hasFailure ? surface.tokens.dangerSoft
                                      : surface.aiBusy ? surface.tokens.focusSoft
                                                       : surface.tokens.surface
            border.width: 1
            border.color: surface.hasFailure ? surface.tokens.danger
                                             : surface.aiBusy ? surface.tokens.focus
                                                              : surface.tokens.border
            Accessible.role: Accessible.AlertMessage
            Accessible.name: surface.statusTitle
            Accessible.description: surface.statusDescription

            Column {
                id: statusColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: surface.tokens.space3
                spacing: surface.tokens.space1

                Text {
                    width: parent.width
                    text: surface.statusTitle
                    color: surface.hasFailure ? surface.tokens.danger : surface.tokens.ink
                    font.family: surface.tokens.fontUi
                    font.pixelSize: surface.tokens.sizeHeading
                    font.weight: Font.DemiBold
                    renderType: Text.NativeRendering
                }

                Text {
                    width: parent.width
                    text: surface.statusDescription
                    color: surface.tokens.textSecondary
                    font.family: surface.tokens.fontUi
                    font.pixelSize: surface.tokens.sizeBody
                    wrapMode: Text.WordWrap
                    renderType: Text.NativeRendering
                }
            }
        }

        Text {
            objectName: "aiPanelDescription"
            Layout.fillWidth: true
            text: surface.projectScope
                  ? "根据当前项目树的选中节点、路径、进度、具体内容和任务数量生成摘要。"
                  : "根据当前便签视图对应阶段的完整快照生成摘要，不受搜索筛选影响。"
            color: surface.tokens.muted
            font.pixelSize: surface.tokens.sizeBody
            font.family: surface.tokens.fontUi
            wrapMode: Text.WordWrap
            renderType: Text.NativeRendering
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 132
            radius: surface.tokens.radiusMd
            color: surface.tokens.drawerCard
            border.color: surface.tokens.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: surface.tokens.space3
                spacing: surface.tokens.space2

                Text {
                    text: "摘要要求"
                    color: surface.tokens.ink
                    font.pixelSize: surface.tokens.sizeHeading
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
                    font.pixelSize: surface.tokens.sizeBody
                    font.family: surface.tokens.fontUi
                    color: surface.tokens.ink
                    activeFocusOnTab: true
                    renderType: Text.NativeRendering
                    Accessible.role: Accessible.EditableText
                    Accessible.name: "摘要要求"
                    Accessible.description: "输入希望智能摘要重点关注的内容"
                    background: Rectangle {
                        radius: surface.tokens.radiusMd
                        color: surface.tokens.surface
                        border.color: requirement.activeFocus ? surface.tokens.focusRing : surface.tokens.border
                        border.width: requirement.activeFocus ? 2 : 1
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
                    accessibleDescription: "使用“" + modelData.label + "”摘要模板"
                    onClicked: requirement.text = modelData.prompt
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            radius: surface.tokens.radiusMd
            color: surface.tokens.drawerCard
            border.color: surface.tokens.border

            Column {
                anchors.fill: parent
                anchors.margins: surface.tokens.space3
                spacing: surface.tokens.space2

                Text {
                    text: surface.projectScope ? "项目上下文" : "便签上下文"
                    color: surface.tokens.ink
                    font.pixelSize: surface.tokens.sizeHeading
                    font.weight: Font.DemiBold
                    font.family: surface.tokens.fontUi
                    renderType: Text.NativeRendering
                }

                Text {
                    objectName: "aiScopeHint"
                    width: parent.width
                    text: surface.projectScope
                          ? (surface.summaryEmpty ? "先创建项目，再生成项目摘要。" : "会使用当前选中节点；未选中时总结整个项目树概况。")
                          : "会使用当前阶段的完整便签快照，不受搜索筛选影响。"
                    color: surface.tokens.muted
                    font.pixelSize: surface.tokens.sizeBody
                    wrapMode: Text.WordWrap
                    font.family: surface.tokens.fontUi
                    renderType: Text.NativeRendering
                }
            }
        }

        RowLayout {
            objectName: "aiActions"
            Layout.fillWidth: true
            spacing: surface.tokens.space2

            Button {
                id: runAi
                objectName: "aiRunButton"
                Layout.fillWidth: true
                Layout.preferredHeight: surface.tokens.primaryControlHeight
                enabled: surface.aiBusy || (!surface.summaryEmpty && surface.hasApiKey)
                hoverEnabled: true
                activeFocusOnTab: true
                clip: true
                onClicked: {
                    if (surface.aiBusy)
                        surface.cancelRequested()
                    else
                        surface.runSummary()
                }
                Accessible.role: Accessible.Button
                Accessible.name: surface.aiBusy ? "取消生成" : "生成摘要"
                Accessible.description: surface.aiBusy
                                        ? "取消当前智能摘要请求"
                                        : !surface.hasApiKey
                                        ? "请先在设置中配置 API Key"
                                        : surface.summaryEmpty
                                          ? "当前没有可用于摘要的内容"
                                          : "根据当前不可变快照生成智能摘要"
                contentItem: Text {
                    text: surface.aiBusy ? "取消生成" : "生成摘要"
                    color: surface.tokens.textOnAccent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: surface.tokens.sizeBody
                    font.weight: Font.Bold
                    font.family: surface.tokens.fontUi
                    renderType: Text.NativeRendering
                }
                background: Rectangle {
                    radius: surface.tokens.radiusMd
                    color: !runAi.enabled ? surface.tokens.textDisabled
                         : surface.aiBusy ? surface.tokens.danger
                         : runAi.pressed ? surface.tokens.accentPressed
                         : runAi.hovered ? surface.tokens.accentHover : surface.tokens.accent
                    border.width: runAi.visualFocus ? 2 : 0
                    border.color: surface.tokens.focusRing
                    Behavior on color { ColorAnimation { duration: surface.tokens.motionFast; easing.type: Easing.OutCubic } }
                }
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
            id: resultArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: true
            text: surface.aiResultText
            wrapMode: TextEdit.WrapAnywhere
            placeholderText: "生成后的摘要会出现在这里。"
            font.pixelSize: surface.tokens.sizeBody
            font.family: surface.tokens.fontUi
            color: surface.tokens.ink
            activeFocusOnTab: true
            renderType: Text.NativeRendering
            Accessible.role: Accessible.EditableText
            Accessible.name: "摘要结果"
            Accessible.description: surface.aiResultText.length > 0
                                    ? "只读的智能摘要结果"
                                    : "摘要生成后会显示在这里"
            Accessible.readOnly: true
            background: Rectangle {
                radius: surface.tokens.radiusLg
                color: surface.tokens.drawerCard
                border.color: resultArea.activeFocus ? surface.tokens.focusRing : surface.tokens.border
                border.width: resultArea.activeFocus ? 2 : 1
            }
        }
    }
}
