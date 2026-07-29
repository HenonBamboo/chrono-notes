pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: panel

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: panel.uiFontFamily
        baseFontSize: panel.uiFontSize
    }
    readonly property var tokens: panel.theme ? panel.theme : panel.fallbackTokens

    property var model
    property int totalCount: 0
    property int completedCount: 0
    property bool hasVisibleRows: false
    property string stageLabel: ""
    property bool searchActive: false
    property string searchQuery: ""
    property bool overlayOpen: false
    property color blueColor: panel.tokens.accentBlue
    property color accentColor: panel.tokens.accentYellow
    property color cardColor: panel.tokens.card
    property color inkColor: panel.tokens.ink
    property color mutedColor: panel.tokens.muted
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    property var collapsedSections: ({})
    property int archiveAutoCollapseThreshold: 6

    signal toggleRequested(int eventId)
    signal deleteRequested(int eventId)
    signal saveRequested(int eventId, string newText)
    signal viewRequested(int eventId, bool readOnly)

    function resetScroll() {
        listView.contentY = 0
    }

    function isArchiveSection(section) {
        return section === "自动收纳"
    }

    function isSectionCollapsed(section) {
        if (!isArchiveSection(section))
            return false
        if (Object.prototype.hasOwnProperty.call(collapsedSections, section))
            return collapsedSections[section] === true
        return Math.max(0, listView.count - panel.totalCount) > archiveAutoCollapseThreshold
    }

    function rowHiddenByCollapse(section, archive) {
        return archive && isSectionCollapsed(section)
    }

    function toggleSectionCollapsed(section) {
        if (!isArchiveSection(section))
            return

        const next = Object.assign({}, collapsedSections)
        next[section] = !isSectionCollapsed(section)
        collapsedSections = next
        listView.forceLayout()
    }

    function sectionTitle(section) {
        return section
    }

    function emptyStateVisible() {
        return !panel.hasVisibleRows && !panel.overlayOpen
    }

    function emptyStateText() {
        if (panel.searchActive) {
            const queryLabel = panel.searchQuery.trim().length > 0
                               ? "“" + panel.searchQuery.trim() + "”"
                               : "当前筛选条件"
            return "没有找到与" + queryLabel + "匹配的便签。"
                    + "\n试试更短的关键词，或切换完成状态筛选。"
        }
        return (panel.stageLabel.length > 0 ? panel.stageLabel + "还没有便签。"
                                             : "这一阶段还没有便签。")
                + "\n在上方写下下一件要做的事，按 Enter 即可添加。"
    }

    Rectangle {
        anchors.fill: parent
        radius: panel.tokens.radiusLg
        color: panel.tokens.surface
        border.width: 1
        border.color: panel.tokens.border
        antialiasing: true
    }

    ProgressStats {
        id: stats
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: panel.tokens.space1
        anchors.rightMargin: panel.tokens.space1
        totalCount: panel.totalCount
        completedCount: panel.completedCount
        blueColor: panel.blueColor
        accentColor: panel.accentColor
        theme: panel.tokens
        uiFontFamily: panel.uiFontFamily
        uiFontSize: panel.uiFontSize
    }

    ListView {
        id: listView
        objectName: "noteListView"
        property bool compact: panel.totalCount > 8 || count > 10
        property bool needsScroll: count > 0 && contentHeight > height + 1
        readonly property NoteRow currentRow: currentItem as NoteRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: stats.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.bottomMargin: 8
        clip: true
        model: panel.model
        spacing: compact ? 7 : 8
        boundsBehavior: Flickable.DragAndOvershootBounds
        interactive: needsScroll
        // ListView already virtualizes off-screen delegates. Reuse remains off
        // because NoteRow owns transient edit/expand state that must never leak
        // to a different note when an item is recycled.
        reuseItems: false
        activeFocusOnTab: true
        keyNavigationEnabled: true
        highlightFollowsCurrentItem: true

        Accessible.role: Accessible.List
        Accessible.name: panel.stageLabel.length > 0 ? panel.stageLabel + "便签列表" : "便签列表"
        Accessible.description: "使用方向键浏览，Enter 打开详情，Space 标记完成"

        Keys.onReturnPressed: if (currentRow) currentRow.viewRequested()
        Keys.onEnterPressed: if (currentRow) currentRow.viewRequested()
        Keys.onSpacePressed: if (currentRow && !currentRow.readOnly) currentRow.toggleRequested()
        section.property: "section"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            required property string section
            width: ListView.view.width
            height: panel.tokens.controlHeight

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: panel.tokens.space2
                anchors.rightMargin: panel.tokens.space2
                radius: panel.tokens.radiusSm
                color: sectionToggle.activeFocus ? panel.tokens.focusSoft : panel.tokens.transparent
                border.width: sectionToggle.activeFocus ? 2 : 0
                border.color: panel.tokens.focusRing
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: panel.tokens.space5
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: panel.sectionTitle(parent.section)
                color: panel.mutedColor
                font.pixelSize: panel.tokens.sizeBody
                font.weight: Font.DemiBold
                font.family: panel.tokens.fontUi
                elide: Text.ElideRight
                renderType: Text.NativeRendering
            }

            MouseArea {
                id: sectionToggle
                anchors.fill: parent
                enabled: panel.isArchiveSection(parent.section)
                activeFocusOnTab: enabled
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: panel.toggleSectionCollapsed(parent.section)

                Accessible.role: Accessible.Button
                Accessible.name: panel.isSectionCollapsed(parent.section)
                                 ? "展开" + parent.section : "收起" + parent.section
                Accessible.description: "切换自动收纳分组的展开状态"

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        panel.toggleSectionCollapsed(parent.section)
                        event.accepted = true
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            policy: listView.needsScroll ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            visible: listView.needsScroll
            width: listView.needsScroll ? 8 : 0
            opacity: (listView.moving || listView.flicking || hovered || pressed) ? 0.7 : 0
            hoverEnabled: true
            Behavior on opacity { NumberAnimation { duration: panel.tokens.motionMedium; easing.type: Easing.OutCubic } }
            contentItem: Rectangle {
                radius: 4
                color: panel.tokens.mutedSoft
            }
            background: Rectangle { color: panel.tokens.transparent }
        }

        populate: Transition {
            NumberAnimation { properties: "y"; duration: panel.tokens.motionSlow; easing.type: Easing.OutCubic }
            NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: panel.tokens.motionMedium }
        }
        displaced: Transition {
            NumberAnimation { properties: "y"; duration: panel.tokens.motionSlow; easing.type: Easing.OutCubic }
        }
        move: Transition {
            NumberAnimation { properties: "y"; duration: panel.tokens.motionSlow; easing.type: Easing.OutCubic }
        }
        moveDisplaced: Transition {
            NumberAnimation { properties: "y"; duration: panel.tokens.motionSlow; easing.type: Easing.OutCubic }
        }
        add: Transition {
            NumberAnimation { property: "y"; from: -18; duration: panel.tokens.motionSlow; easing.type: Easing.OutCubic }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: panel.tokens.motionSlow }
        }
        remove: Transition {
            NumberAnimation { property: "x"; to: 28; duration: panel.tokens.motionMedium; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; to: 0; duration: panel.tokens.motionMedium }
        }

        delegate: NoteRow {
            width: listView.width
            compact: listView.compact
            blueColor: panel.blueColor
            cardColor: panel.cardColor
            inkColor: panel.inkColor
            mutedColor: panel.mutedColor
            theme: panel.tokens
            uiFontFamily: panel.uiFontFamily
            uiFontSize: panel.uiFontSize
            collapsedBySection: panel.rowHiddenByCollapse(section, archive)
            onToggleRequested: panel.toggleRequested(eventId)
            onDeleteRequested: panel.deleteRequested(eventId)
            onSaveRequested: function(newText) {
                panel.saveRequested(eventId, newText)
            }
            onViewRequested: panel.viewRequested(eventId, readOnly)
        }

        Text {
            objectName: "noteListEmptyState"
            anchors.centerIn: parent
            width: Math.min(parent.width - 60, 430)
            visible: panel.emptyStateVisible()
            text: panel.emptyStateText()
            color: panel.tokens.textSecondary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: panel.tokens.sizeBody
            lineHeight: 1.4
            font.family: panel.tokens.fontUi
            renderType: Text.NativeRendering
            Accessible.role: Accessible.StaticText
            Accessible.name: panel.searchActive ? "没有搜索结果" : "当前阶段没有便签"
            Accessible.description: text
        }
    }
}
