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
        uiFontFamily: "Microsoft YaHei UI"
        uiFontSize: 12
        uiFontFamilies: ["Microsoft YaHei UI", "Segoe UI", "SimSun"]
    }

    SignalSpy {
        id: saveSpy
        target: panel
        signalName: "saveRequested"
    }

    function init() {
        panel.apiExpanded = true
        panel.dataExpanded = false
        panel.appearanceExpanded = true
        panel.clearAllArmed = false
        panel.surfaceColor = panel.tokens.drawerPaper
        panel.uiFontFamily = "Microsoft YaHei UI"
        panel.uiFontSize = 12
        saveSpy.clear()
        wait(0)
    }

    function test_resetTextViewsKeepsLongApiUrlReadableFromStart() {
        panel.resetTextViews()
        compare(panel.apiUrlCursorPosition(), 0)
    }

    function test_settingsSectionsExposeClearHierarchy() {
        compare(panel.apiSectionExpanded(), true)
        compare(panel.dataSectionExpanded(), false)
        compare(panel.appearanceSectionExpanded(), true)

        panel.toggleDataSection()
        compare(panel.dataSectionExpanded(), true)

        panel.toggleApiSection()
        compare(panel.apiSectionExpanded(), false)
    }

    function test_settingsCopyMakesScopeExplicitAndReadable() {
        panel.dataExpanded = true
        wait(0)

        compare(findChild(panel, "settingsTitle").text, "设置")
        compare(findChild(panel, "settingsApiTitle").text, "AI 接口")
        compare(findChild(panel, "settingsAppearanceTitle").text, "界面")
        compare(findChild(panel, "settingsDataTitle").text, "便签数据")
        verify(findChild(panel, "settingsDataDescription").text.indexOf("只作用于便签数据") >= 0)
        compare(findChild(panel, "clearCurrentStickiesButton").text, "清空当前便签阶段")
        compare(findChild(panel, "exportStickiesJsonButton").text, "导出便签 JSON")
        compare(findChild(panel, "importStickiesJsonButton").text, "导入便签 JSON")
    }

    function test_settingsSurfaceUsesDrawerPaperNotWorkspacePaper() {
        const surface = findChild(panel, "settingsPanelSurface") || panel
        compare(surface.color, panel.tokens.drawerPaper)
        verify(surface.color !== panel.tokens.paper)
        verify(surface.color !== panel.tokens.paperProject)
    }

    function test_fontSettingsUseSystemFontPickerAndSavedValue() {
        const fontCombo = findChild(panel, "settingsFontFamilyCombo")
        const oldField = findChild(panel, "settingsFontFamilyField")
        const sizeField = findChild(panel, "settingsFontSizeField")
        const saveButton = findChild(panel, "settingsSaveButton")

        verify(fontCombo !== null)
        compare(oldField, null)
        verify(sizeField !== null)
        compare(fontCombo.currentText, "Microsoft YaHei UI")
        compare(sizeField.value, 12)

        fontCombo.currentIndex = 1
        sizeField.value = 15
        compare(panel.uiFontFamily, "Microsoft YaHei UI")
        compare(panel.uiFontSize, 12)
        saveButton.clicked()

        compare(saveSpy.count, 1)
        compare(saveSpy.signalArguments[0][3], "Segoe UI")
        compare(saveSpy.signalArguments[0][4], 15)
    }

    function test_externalFontBindingCanResyncPickerAfterDraftChange() {
        const fontCombo = findChild(panel, "settingsFontFamilyCombo")
        const sizeField = findChild(panel, "settingsFontSizeField")

        fontCombo.currentIndex = 1
        sizeField.value = 15
        panel.uiFontFamily = "SimSun"
        panel.uiFontSize = 16
        wait(0)

        compare(fontCombo.currentText, "SimSun")
        compare(sizeField.value, 16)
    }
}
