import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "AppTitleBar"
    when: windowShown

    AppTitleBar {
        id: titlebar
        width: 900
        height: 40
        titleText: "ChronoNotes · 项目树"
    }

    function test_titlebarUsesStickyNameAndNoDecorativeDots() {
        compare(findChild(titlebar, "stickiesWorkspaceButton").text, "便签")
        compare(findChild(titlebar, "projectsWorkspaceButton").text, "项目树")
        verify(findChild(titlebar, "titlebarDecorDots") === null)
        verify(findChild(titlebar, "titleTextLabel") === null)
    }

    function test_globalActionsRemainAvailable() {
        const aiButton = findChild(titlebar, "aiSummaryButton")
        const settingsButton = findChild(titlebar, "settingsButton")
        compare(aiButton.text, "智能摘要")
        compare(settingsButton.text, "设置")
        compare(aiButton.active, false)
        compare(aiButton.primary, false)
        compare(settingsButton.active, false)

        titlebar.activePanel = "ai"
        compare(aiButton.active, true)
        compare(aiButton.primary, true)
    }
}
