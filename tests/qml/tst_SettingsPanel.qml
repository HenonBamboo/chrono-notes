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
        newApiKey: ""
        hasApiKey: true
        modelName: "gpt-4o-mini"
        allowLocalHttp: false
        reduceMotion: false
        dataDirectory: "C:/Users/Test/AppData/Local/ChronoNotes"
        backupEntries: [
            {
                name: "daily-2026-07-26.chrononotes",
                url: "file:///C:/backups/daily-2026-07-26.chrononotes"
            }
        ]
        uiFontFamily: "Microsoft YaHei UI"
        uiFontSize: 12
        uiFontFamilies: ["Microsoft YaHei UI", "Segoe UI", "SimSun"]
    }

    SignalSpy {
        id: saveSpy
        target: panel
        signalName: "saveRequested"
    }

    SignalSpy {
        id: clearKeySpy
        target: panel
        signalName: "clearApiKeyRequested"
    }

    SignalSpy {
        id: restoreSpy
        target: panel
        signalName: "restoreBackupRequested"
    }

    function init() {
        panel.apiExpanded = true
        panel.recoveryExpanded = false
        panel.appearanceExpanded = true
        panel.clearAllArmed = false
        panel.surfaceColor = panel.tokens.drawerPaper
        panel.uiFontFamily = "Microsoft YaHei UI"
        panel.uiFontSize = 12
        saveSpy.clear()
        clearKeySpy.clear()
        restoreSpy.clear()
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
        panel.recoveryExpanded = true
        wait(0)

        compare(findChild(panel, "settingsTitle").text, "设置")
        compare(findChild(panel, "settingsDataTitle").text, "数据与恢复")
        verify(findChild(panel, "settingsDataDescription").text.indexOf("不会包含 API Key") >= 0)
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
        compare(saveSpy.signalArguments[0][5], "Segoe UI")
        compare(saveSpy.signalArguments[0][6], 15)
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

    function test_credentialsAreWriteOnlyAndSecurityOptionsAreExplicit() {
        const keyField = findChild(panel, "apiKeyField")
        const status = findChild(panel, "apiCredentialStatus")
        const clearButton = findChild(panel, "clearApiKeyButton")
        const httpCheck = findChild(panel, "allowLocalHttpCheck")

        verify(keyField !== null)
        compare(keyField.text, "")
        verify(keyField.placeholderText.indexOf("留空保持不变") >= 0)
        verify(status !== null)
        compare(panel.hasApiKey, true)
        verify(clearButton.enabled)
        compare(httpCheck.checked, false)

        clearButton.clicked()
        compare(clearKeySpy.count, 1)
    }

    function test_recoveryCenterShowsBackupsAndRequiresExplicitRestore() {
        panel.recoveryExpanded = true
        wait(0)

        const backupPicker = findChild(panel, "backupPicker")
        const restoreButton = findChild(panel, "restoreBackupButton")
        const diagnosticsButton = findChild(panel, "exportDiagnosticsButton")
        verify(backupPicker !== null)
        verify(restoreButton.enabled)
        verify(diagnosticsButton !== null)

        restoreButton.clicked()
        compare(restoreSpy.count, 1)
        verify(String(restoreSpy.signalArguments[0][0]).indexOf("daily-2026-07-26") >= 0)
    }
}
