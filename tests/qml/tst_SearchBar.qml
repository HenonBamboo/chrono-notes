import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "SearchBar"
    when: windowShown

    SearchBar {
        id: search
        width: 420
        open: true
        query: "needle"
        completionFilter: -1
    }

    SignalSpy {
        id: querySpy
        target: search
        signalName: "queryEdited"
    }

    SignalSpy {
        id: closeSpy
        target: search
        signalName: "closeRequested"
    }

    function init() {
        search.open = true
        search.query = ""
        search.query = "needle"
        search.completionFilter = -1
        querySpy.clear()
        closeSpy.clear()
    }

    function test_clearKeepsSearchOpenAndFocusesInput() {
        search.clearAndFocus()

        compare(querySpy.count, 1)
        compare(querySpy.signalArguments[0][0], "")
        compare(closeSpy.count, 0)
        tryCompare(search, "inputActiveFocus", true)
    }

    function test_escapeClearsBeforeClosing() {
        search.forceSearchFocus()
        tryCompare(search, "inputActiveFocus", true)

        keyClick(Qt.Key_Escape)

        compare(querySpy.count, 1)
        compare(querySpy.signalArguments[0][0], "")
        compare(closeSpy.count, 0)
    }

    function test_escapeClosesWhenAlreadyEmpty() {
        search.query = ""
        search.forceSearchFocus()
        tryCompare(search, "inputActiveFocus", true)

        keyClick(Qt.Key_Escape)

        compare(querySpy.count, 0)
        compare(closeSpy.count, 1)
    }
}
