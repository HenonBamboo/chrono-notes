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

    function test_globalActionsShareTheSameActiveModel() {
        const aiButton = findChild(titlebar, "aiSummaryButton")
        const settingsButton = findChild(titlebar, "settingsButton")
        compare(aiButton.text, "智能摘要")
        compare(settingsButton.text, "设置")
        compare(aiButton.active, false)
        compare(aiButton.primary, false)
        compare(settingsButton.active, false)
        compare(settingsButton.primary, false)

        titlebar.activePanel = "ai"
        compare(aiButton.active, true)
        compare(aiButton.primary, false)
        compare(settingsButton.active, false)
        compare(settingsButton.primary, false)

        titlebar.activePanel = "settings"
        compare(aiButton.active, false)
        compare(aiButton.primary, false)
        compare(settingsButton.active, true)
        compare(settingsButton.primary, false)
    }

    function test_titlebarAvoidsDuplicatingNativeWindowIdentity() {
        const notesButton = findChild(titlebar, "stickiesWorkspaceButton")
        const minimizeButton = findChild(titlebar, "minimizeWindowButton")
        const maximizeButton = findChild(titlebar, "maximizeWindowButton")
        const closeButton = findChild(titlebar, "closeWindowButton")

        verify(findChild(titlebar, "brandNameLabel") === null)
        verify(notesButton.activeFocusOnTab)
        verify(notesButton.implicitHeight >= 40)
        compare(minimizeButton.accessibleName, "最小化")
        compare(maximizeButton.accessibleName, "最大化窗口")
        compare(closeButton.accessibleName, "关闭")
        verify(minimizeButton.implicitWidth >= 40)
        verify(maximizeButton.implicitWidth >= 40)
        verify(closeButton.implicitWidth >= 40)
    }
}
