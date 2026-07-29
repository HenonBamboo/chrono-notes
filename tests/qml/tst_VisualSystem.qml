import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "VisualSystem"
    when: windowShown

    Item {
        id: harness
        width: 640
        height: 480

        ChronoTokens {
            id: tokens
        }

        ToolPill {
            id: toolButton
            text: "测试操作"
            accessibleDescription: "执行测试操作"
            theme: tokens
        }

        WindowButton {
            id: windowButton
            anchors.top: toolButton.bottom
            label: "−"
            accessibleName: "最小化"
            theme: tokens
        }

        DetailPanel {
            id: detailPanel
            anchors.top: windowButton.bottom
            width: 360
            height: 320
            eventText: "测试便签"
            eventMeta: "今天"
            theme: tokens
        }
    }

    function test_corePaletteAndTypeScaleAreProductionDefaults() {
        compare(tokens.canvas, "#f7f2e8")
        compare(tokens.surface, "#fffdf8")
        compare(tokens.surfaceMuted, "#f2ebdd")
        compare(tokens.ink, "#202723")
        compare(tokens.textSecondary, "#5e6862")
        compare(tokens.border, "#d8d0c3")
        compare(tokens.accent, "#82551f")
        compare(tokens.accentSoft, "#f1dfc0")
        compare(tokens.projectAccent, "#58705a")
        compare(tokens.focus, "#245ea8")
        compare(tokens.danger, "#9b3730")
        compare(tokens.dangerSoft, "#f8e2df")
        compare(tokens.sizeBody, 14)
        compare(tokens.sizeMeta, 12)
        compare(tokens.sizeTitle, 16)
        compare(tokens.sizeDisplay, 20)
    }

    function test_interactiveControlsMeetKeyboardAndTargetBaselines() {
        verify(toolButton.activeFocusOnTab)
        verify(toolButton.implicitHeight >= 40)
        compare(toolButton.accessibleDescription, "执行测试操作")

        verify(windowButton.activeFocusOnTab)
        verify(windowButton.implicitWidth >= 40)
        verify(windowButton.implicitHeight >= 40)
        compare(windowButton.accessibleName, "最小化")

        const editor = findChild(detailPanel, "detailEditor")
        const saveButton = findChild(detailPanel, "detailSaveButton")
        verify(editor !== null)
        verify(saveButton !== null)
        verify(editor.activeFocusOnTab)
        verify(saveButton.activeFocusOnTab)
        verify(saveButton.height >= 44)
    }

    function test_keyboardFocusUsesDedicatedFocusColor() {
        toolButton.forceActiveFocus(Qt.TabFocusReason)
        tryCompare(toolButton, "activeFocus", true)
        tryCompare(toolButton, "visualFocus", true)
        compare(toolButton.focusBorderWidth, 2)
        compare(tokens.focusRing, tokens.focus)
    }

    function test_reduceMotionDisablesSharedTransitions() {
        verify(tokens.motionFast > 0)
        verify(tokens.motionMedium > 0)
        verify(tokens.drawerDuration > 0)

        tokens.reduceMotion = true

        compare(tokens.motionFast, 0)
        compare(tokens.motionMedium, 0)
        compare(tokens.drawerDuration, 0)
    }
}
