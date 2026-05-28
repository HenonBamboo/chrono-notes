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
}
