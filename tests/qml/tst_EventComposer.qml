import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "EventComposer"
    when: windowShown

    EventComposer {
        id: composer
        width: 420
        height: 42
        stage: 0
        inkColor: "#071426"
    }

    function test_forceComposerFocus_focusesInput() {
        composer.forceComposerFocus()
        tryCompare(composer, "inputActiveFocus", true)
    }

    function test_inputSupportsKeyboardAndProductionTextLimit() {
        const input = findChild(composer, "composerInput")
        verify(input !== null)
        verify(input.activeFocusOnTab)
        compare(input.maximumLength, 65536)
        verify(input.height >= 40)
    }
}
