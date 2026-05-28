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
}
