import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "SettingsPanel"
    when: windowShown

    SettingsPanel {
        id: panel
        width: 360
        height: 520
        apiUrl: "https://api.openai.com/v1/chat/completions"
        apiKey: ""
        modelName: "gpt-4o-mini"
    }

    function init() {
        panel.apiExpanded = true
        panel.dataExpanded = false
        panel.clearAllArmed = false
        panel.surfaceColor = "#fff5af"
    }

    function test_resetTextViewsKeepsLongApiUrlReadableFromStart() {
        panel.resetTextViews()
        compare(panel.apiUrlCursorPosition(), 0)
    }

    function test_settingsSectionsExposeClearHierarchy() {
        compare(panel.apiSectionExpanded(), true)
        compare(panel.dataSectionExpanded(), false)

        panel.toggleDataSection()
        compare(panel.dataSectionExpanded(), true)

        panel.toggleApiSection()
        compare(panel.apiSectionExpanded(), false)
    }

    function test_settingsCopyMakesScopeExplicit() {
        panel.dataExpanded = true
        wait(0)

        compare(findChild(panel, "settingsTitle").text, "设置")
        compare(findChild(panel, "settingsApiTitle").text, "AI 接口")
        compare(findChild(panel, "settingsDataTitle").text, "便签数据")
        verify(findChild(panel, "settingsDataDescription").text.indexOf("只作用于便签") >= 0)
        compare(findChild(panel, "clearCurrentStickiesButton").text, "清空当前便签阶段")
        compare(findChild(panel, "exportStickiesJsonButton").text, "导出便签 JSON")
        compare(findChild(panel, "importStickiesJsonButton").text, "导入便签 JSON")
    }

    function test_settingsSurfaceCanFollowProjectWorkspace() {
        panel.surfaceColor = "#e9f7d7"
        const surface = findChild(panel, "settingsPanelSurface") || panel
        compare(surface.color, "#e9f7d7")
    }
}
