pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: panel

    property var model
    property int totalCount: 0
    property int completedCount: 0
    property bool hasVisibleRows: false
    property string stageLabel: ""
    property bool overlayOpen: false
    property color blueColor: "#2d68c7"
    property color accentColor: "#f4bf30"
    property color cardColor: "#fffef7"
    property color inkColor: "#071426"
    property color mutedColor: "#64748b"
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
        radius: 26
        color: "#12fffef7"
        border.width: 0
        antialiasing: true
    }

    ProgressStats {
        id: stats
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 4
        anchors.rightMargin: 4
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
        spacing: compact ? 8 : 10
        boundsBehavior: Flickable.DragAndOvershootBounds
        interactive: needsScroll
        reuseItems: false
        section.property: "section"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            required property string section
            width: ListView.view.width
            height: 34

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 28
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: panel.sectionTitle(parent.section)
                color: "#40577a"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.family: "Microsoft YaHei UI"
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
                color: "#9aa36d"
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
            text: panel.stageLabel + "还没有事件。\n写下一件真正要做的事就够了。"
            color: "#8290a7"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: 14
            lineHeight: 1.4
            font.family: "Microsoft YaHei UI"
            renderType: Text.NativeRendering
        }
    }
}
