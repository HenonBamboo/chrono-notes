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

    function init() {
        panel.summaryScope = "stickies"
        panel.summaryContextText = ""
        panel.summaryEmpty = false
        panel.surfaceColor = "#fff5af"
        panel.aiBusy = false
        panel.aiResultText = ""
        runSpy.clear()
    }

    function test_stickiesScopeUsesStickyCopy() {
        compare(findChild(panel, "aiPanelTitle").text, "便签摘要")
        verify(findChild(panel, "aiPanelDescription").text.indexOf("当前便签视图") >= 0)
        verify(findChild(panel, "aiRequirementInput").text.indexOf("便签") >= 0)
        verify(findChild(panel, "aiRunButton").enabled)
    }

    function test_projectScopeUsesProjectCopyContextAndMintSurface() {
        panel.summaryScope = "projects"
        panel.summaryContextText = "项目摘要上下文\n标题：产品改版\n具体内容：右侧说明"
        panel.surfaceColor = "#e9f7d7"
        wait(0)

        compare(findChild(panel, "aiPanelTitle").text, "项目摘要")
        verify(findChild(panel, "aiPanelDescription").text.indexOf("当前项目树") >= 0)
        verify(findChild(panel, "aiRequirementInput").text.indexOf("项目") >= 0)
        const surface = findChild(panel, "aiPanelSurface") || panel
        compare(surface.color, "#e9f7d7")

        panel.runSummary()
        compare(runSpy.count, 1)
        compare(runSpy.signalArguments[0][1], "项目摘要上下文\n标题：产品改版\n具体内容：右侧说明")
    }

    function test_emptyProjectSummaryIsDisabled() {
        panel.summaryScope = "projects"
        panel.summaryEmpty = true
        wait(0)

        compare(findChild(panel, "aiRunButton").enabled, false)
        verify(findChild(panel, "aiScopeHint").text.indexOf("先创建项目") >= 0)
    }
}
