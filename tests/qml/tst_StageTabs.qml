import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "StageTabs"
    when: windowShown

    StageTabs {
        id: tabs
        width: 520
        stage: 0
    }

    SignalSpy {
        id: stageSpy
        target: tabs
        signalName: "stageRequested"
    }

    SignalSpy {
        id: toggleSpy
        target: tabs
        signalName: "toggleAllRequested"
    }

    function init() {
        tabs.stage = 0
        tabs.allCompleted = false
        stageSpy.clear()
        toggleSpy.clear()
    }

    function test_stageNavigationIsCompactAndKeyboardAccessible() {
        const dayButton = findChild(tabs, "stageTab0")
        const weekButton = findChild(tabs, "stageTab1")

        verify(dayButton !== null)
        verify(weekButton !== null)
        verify(dayButton.implicitHeight <= 34)
        verify(dayButton.activeFocusOnTab)
        compare(dayButton.current, true)
        compare(weekButton.current, false)

        weekButton.clicked()
        compare(stageSpy.count, 1)
        compare(stageSpy.signalArguments[0][0], 1)
    }

    function test_bulkCompletionLivesInMenu() {
        const menuButton = findChild(tabs, "stageBulkMenuButton")
        const toggleItem = findChild(tabs, "stageToggleAllMenuItem")

        verify(menuButton !== null)
        verify(menuButton.activeFocusOnTab)
        verify(toggleItem !== null)
        compare(toggleItem.text, "完成当前阶段全部便签")

        tabs.allCompleted = true
        compare(toggleItem.text, "取消当前阶段全部完成")
        toggleItem.triggered()
        compare(toggleSpy.count, 1)
    }
}
