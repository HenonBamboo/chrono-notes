pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: panel

    readonly property ChronoTokens tokens: ChronoTokens {}

    property var model
    property int totalCount: 0
    property int completedCount: 0
    property bool hasVisibleRows: false
    property string stageLabel: ""
    property bool overlayOpen: false
    property color blueColor: tokens.accentBlue
    property color accentColor: tokens.accentYellow
    property color cardColor: tokens.card
    property color inkColor: tokens.ink
    property color mutedColor: tokens.muted
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

    Rectangle {
        anchors.fill: parent
        radius: tokens.radiusLg
        color: "#22fffef7"
        border.width: 1
        border.color: tokens.lineSoft
        antialiasing: true
    }

    ProgressStats {
        id: stats
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: tokens.space1
        anchors.rightMargin: tokens.space1
        totalCount: panel.totalCount
        completedCount: panel.completedCount
        blueColor: panel.blueColor
        accentColor: panel.accentColor
    }

    ListView {
        id: listView
        property bool compact: panel.totalCount > 8 || count > 10
        property bool needsScroll: count > 0 && contentHeight > height + 1

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
        reuseItems: false
        section.property: "section"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            required property string section
            width: ListView.view.width
            height: 30

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 28
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: panel.sectionTitle(parent.section)
                color: panel.mutedColor
                font.pixelSize: tokens.sizeBody
                font.weight: Font.DemiBold
                font.family: tokens.fontUi
                elide: Text.ElideRight
                renderType: Text.NativeRendering
            }

            MouseArea {
                anchors.fill: parent
                enabled: panel.isArchiveSection(parent.section)
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: panel.toggleSectionCollapsed(parent.section)
            }
        }

        ScrollBar.vertical: ScrollBar {
            policy: listView.needsScroll ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            visible: listView.needsScroll
            width: listView.needsScroll ? 8 : 0
            opacity: (listView.moving || listView.flicking || hovered || pressed) ? 0.7 : 0
            hoverEnabled: true
            Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            contentItem: Rectangle {
                radius: 4
                color: tokens.mutedSoft
            }
            background: Rectangle { color: "transparent" }
        }

        populate: Transition {
            NumberAnimation { properties: "y"; duration: 250; easing.type: Easing.OutCubic }
            NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: 180 }
        }
        displaced: Transition {
            NumberAnimation { properties: "y"; duration: 340; easing.type: Easing.OutCubic }
        }
        move: Transition {
            NumberAnimation { properties: "y"; duration: 360; easing.type: Easing.OutCubic }
        }
        moveDisplaced: Transition {
            NumberAnimation { properties: "y"; duration: 360; easing.type: Easing.OutCubic }
        }
        add: Transition {
            NumberAnimation { property: "y"; from: -18; duration: 320; easing.type: Easing.OutCubic }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 220 }
        }
        remove: Transition {
            NumberAnimation { property: "x"; to: 28; duration: 190; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; to: 0; duration: 170 }
        }

        delegate: NoteRow {
            width: listView.width
            compact: listView.compact
            blueColor: panel.blueColor
            cardColor: panel.cardColor
            inkColor: panel.inkColor
            mutedColor: panel.mutedColor
            collapsedBySection: panel.rowHiddenByCollapse(section, archive)
            onToggleRequested: panel.toggleRequested(eventId)
            onDeleteRequested: panel.deleteRequested(eventId)
            onSaveRequested: function(newText) {
                panel.saveRequested(eventId, newText)
            }
            onViewRequested: panel.viewRequested(eventId, readOnly)
        }

        Text {
            anchors.centerIn: parent
            width: Math.min(parent.width - 60, 430)
            visible: panel.emptyStateVisible()
            text: panel.stageLabel + "还没有便签。\n写下一件真正要做的事就够了。"
            color: tokens.mutedSoft
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: 14
            lineHeight: 1.4
            font.family: tokens.fontUi
            renderType: Text.NativeRendering
        }
    }
}
