import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "AiSummaryPanel"
    when: windowShown

    AiSummaryPanel {
        id: panel
        width: 320
        height: 520
    }

    SignalSpy {
        id: runSpy
        target: panel
        signalName: "runRequested"
    }

    SignalSpy {
        id: cancelSpy
        target: panel
        signalName: "cancelRequested"
    }

    function init() {
        panel.summaryScope = "stickies"
        panel.summaryContextText = ""
        panel.summaryEmpty = false
        panel.surfaceColor = panel.tokens.drawerPaper
        panel.hasApiKey = true
        panel.aiBusy = false
        panel.aiState = "idle"
        panel.aiError = ""
        panel.aiResultText = ""
        runSpy.clear()
        cancelSpy.clear()
    }

    function test_stickiesScopeUsesStickyCopy() {
        compare(findChild(panel, "aiPanelTitle").text, "便签摘要")
        verify(findChild(panel, "aiPanelDescription").text.indexOf("当前便签视图") >= 0)
        verify(findChild(panel, "aiRequirementInput").text.indexOf("便签") >= 0)
        verify(findChild(panel, "aiRunButton").enabled)
    }

    function test_projectScopeUsesProjectCopyContextAndDrawerSurface() {
        panel.summaryScope = "projects"
        panel.summaryContextText = "项目摘要上下文\n标题：产品重构\n具体内容：右侧说明"
        wait(0)

        compare(findChild(panel, "aiPanelTitle").text, "项目摘要")
        verify(findChild(panel, "aiPanelDescription").text.indexOf("当前项目树") >= 0)
        verify(findChild(panel, "aiRequirementInput").text.indexOf("项目树") >= 0)
        const surface = findChild(panel, "aiPanelSurface") || panel
        compare(surface.color, panel.tokens.drawerPaper)
        verify(surface.color !== panel.tokens.paper)
        verify(surface.color !== panel.tokens.paperProject)

        panel.runSummary()
        compare(runSpy.count, 1)
        compare(runSpy.signalArguments[0][1], "项目摘要上下文\n标题：产品重构\n具体内容：右侧说明")
    }

    function test_emptyProjectSummaryIsDisabled() {
        panel.summaryScope = "projects"
        panel.summaryEmpty = true
        wait(0)

        compare(findChild(panel, "aiRunButton").enabled, false)
        verify(findChild(panel, "aiScopeHint").text.indexOf("先创建项目") >= 0)
    }

    function test_missingCredentialAndFailureExposeClearStates() {
        panel.hasApiKey = false
        wait(0)

        compare(findChild(panel, "aiRunButton").enabled, false)
        compare(panel.statusTitle, "尚未配置 AI")
        verify(panel.statusDescription.indexOf("设置") >= 0)

        panel.hasApiKey = true
        panel.aiState = "error"
        panel.aiError = "请求超时"
        wait(0)

        compare(panel.statusTitle, "生成失败")
        compare(panel.statusDescription, "请求超时")
    }

    function test_runningRequestCanBeCancelled() {
        panel.aiBusy = true
        panel.aiState = "running"
        wait(0)

        const runButton = findChild(panel, "aiRunButton")
        compare(runButton.contentItem.text, "取消生成")
        verify(runButton.enabled)
        runButton.clicked()
        compare(cancelSpy.count, 1)
    }
}
