import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "NoteListPanel"
    when: windowShown

    NoteListPanel {
        id: panel
        width: 520
        height: 420
        model: ListModel {
            ListElement {
                eventId: 1
                text: "manual"
                completed: false
                meta: "今天"
                section: "待处理"
                sectionFirst: true
                readOnly: false
                archive: false
                highlightedText: "manual"
                repeat: ""
            }
            ListElement {
                eventId: 2
                text: "archive"
                completed: false
                meta: "来自 2026-05-24"
                section: "自动收纳"
                sectionFirst: true
                readOnly: true
                archive: true
                highlightedText: "archive"
                repeat: ""
            }
        }
        totalCount: 1
        completedCount: 0
        hasVisibleRows: true
        stageLabel: "每周"
    }

    function test_archiveSectionsCanBeCollapsedWithoutHidingManualRows() {
        const archiveSection = "自动收纳"

        compare(panel.isSectionCollapsed(archiveSection), false)
        compare(panel.rowHiddenByCollapse("待处理", false), false)
        compare(panel.rowHiddenByCollapse(archiveSection, true), false)

        panel.toggleSectionCollapsed(archiveSection)

        compare(panel.isSectionCollapsed(archiveSection), true)
        compare(panel.rowHiddenByCollapse("待处理", false), false)
        compare(panel.rowHiddenByCollapse(archiveSection, true), true)

        panel.toggleSectionCollapsed(archiveSection)

        compare(panel.isSectionCollapsed(archiveSection), false)
        compare(panel.rowHiddenByCollapse(archiveSection, true), false)
    }

    function test_archiveSectionsAutoCollapseWhenHistoryIsDense() {
        const archiveSection = "自动收纳"
        panel.archiveAutoCollapseThreshold = 0
        compare(panel.isSectionCollapsed(archiveSection), true)
        compare(panel.rowHiddenByCollapse(archiveSection, true), true)

        panel.toggleSectionCollapsed(archiveSection)
        compare(panel.isSectionCollapsed(archiveSection), false)
        compare(panel.rowHiddenByCollapse(archiveSection, true), false)
    }

    function test_archiveSectionTitleDoesNotExposeStateLetter() {
        compare(panel.sectionTitle("自动收纳"), "自动收纳")
        compare(panel.sectionTitle("待处理"), "待处理")
    }

    function test_emptyStateHidesWhenOnlyArchiveRowsExist() {
        compare(panel.emptyStateVisible(), false)
    }

    function test_emptyStateHidesWhileOverlayIsOpen() {
        panel.hasVisibleRows = false
        panel.overlayOpen = true

        compare(panel.emptyStateVisible(), false)
    }

    function test_searchEmptyStateExplainsNoResults() {
        panel.hasVisibleRows = false
        panel.overlayOpen = false
        panel.searchActive = true
        panel.searchQuery = "发布说明"

        const emptyState = findChild(panel, "noteListEmptyState")
        verify(panel.emptyStateVisible())
        verify(emptyState.text.indexOf("没有找到") >= 0)
        verify(emptyState.text.indexOf("发布说明") >= 0)
        compare(emptyState.Accessible.name, "没有搜索结果")
    }

    function test_listAndRowActionsMeetKeyboardTargetBaseline() {
        const list = findChild(panel, "noteListView")
        const completeButton = findChild(panel, "completeNoteButton")
        const deleteButton = findChild(panel, "deleteNoteButton")
        const viewButton = findChild(panel, "viewNoteButton")

        verify(list !== null)
        verify(list.activeFocusOnTab)
        verify(completeButton !== null)
        verify(deleteButton !== null)
        verify(viewButton !== null)
        verify(completeButton.implicitHeight >= 40)
        verify(deleteButton.implicitHeight >= 40)
        verify(viewButton.implicitHeight >= 40)
        verify(completeButton.activeFocusOnTab)
        verify(deleteButton.activeFocusOnTab)
        verify(viewButton.activeFocusOnTab)
    }
}
