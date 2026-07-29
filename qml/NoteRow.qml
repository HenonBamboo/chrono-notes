import QtQuick
import QtQuick.Controls

Rectangle {
    id: row

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: row.uiFontFamily
        baseFontSize: row.uiFontSize
    }
    readonly property var tokens: row.theme ? row.theme : row.fallbackTokens

    required property int eventId
    required property string text
    required property bool completed
    required property string meta
    required property string section
    required property bool sectionFirst
    required property bool readOnly
    required property bool archive
    required property string highlightedText
    required property string repeat

    property color cardColor: row.tokens.card
    property color inkColor: row.tokens.ink
    property color mutedColor: row.tokens.muted
    property color blueColor: row.tokens.accentBlue
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    property bool compact: false
    property bool editing: false
    property bool expanded: false
    property bool collapsedBySection: false
    property string repeatLabel: repeat === "daily" ? "每天"
                                 : repeat === "weekly" ? "每周"
                                 : repeat === "monthly" ? "每月"
                                 : repeat === "yearly" ? "每年"
                                 : ""
    property bool hovering: rowHover.containsMouse || actionRow.hovering
    property int contentInset: archive ? 16 : 18
    property int actionGutter: archive ? 100 : 152

    signal toggleRequested()
    signal deleteRequested()
    signal saveRequested(string newText)
    signal viewRequested()

    height: {
        if (collapsedBySection)
            return 0
        if (editing)
            return Math.max(82, editor.contentHeight + 34)
        if (archive)
            return expanded ? Math.max(112, archiveContent.implicitHeight + 30) : (compact ? 70 : 78)
        return Math.max(compact && !expanded ? 62 : 78, normalContent.implicitHeight + (compact && !expanded ? 24 : 32))
    }
    radius: 0
    color: row.tokens.transparent
    visible: !collapsedBySection
    opacity: completed || archive ? 0.9 : 1
    border.width: 0
    antialiasing: true
    clip: true
    activeFocusOnTab: !row.editing && row.visible

    Accessible.role: Accessible.ListItem
    Accessible.name: row.text
    Accessible.description: row.archive ? "自动收纳便签，只读，按 Enter 查看详情"
                            : row.completed ? "已完成便签，按 Enter 查看详情"
                            : "未完成便签，按 Enter 查看详情，按 Space 标记完成"

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            row.viewRequested()
            event.accepted = true
        } else if (event.key === Qt.Key_Space && !row.readOnly) {
            row.toggleRequested()
            event.accepted = true
        }
    }

    Behavior on height { NumberAnimation { duration: row.tokens.motionSlow; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: row.tokens.motionMedium; easing.type: Easing.OutCubic } }
    Behavior on y { NumberAnimation { duration: row.tokens.motionSlow; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: row.tokens.radiusMd
        color: row.archive ? row.tokens.archiveSurface
             : row.completed ? row.tokens.completedSurface : row.cardColor
        border.width: row.activeFocus ? 2 : 1
        border.color: row.activeFocus ? row.tokens.focusRing
                    : row.hovering ? row.tokens.accent : row.tokens.borderSubtle
        antialiasing: true
        clip: true
        Behavior on border.color { ColorAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        width: 3
        radius: 2
        anchors.left: card.left
        anchors.top: card.top
        anchors.bottom: card.bottom
        color: row.archive ? row.tokens.textDisabled
             : row.completed ? row.tokens.projectAccent : row.tokens.accent
        opacity: row.hovering || row.completed ? 0.95 : 0
        Behavior on opacity { NumberAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
    }

    function startEdit() {
        if (row.readOnly) {
            row.expanded = !row.expanded
            return
        }
        editing = true
        editor.text = row.text
        Qt.callLater(function() {
            editor.forceActiveFocus()
            editor.cursorPosition = editor.length
        })
    }

    function commitEdit() {
        if (!editing)
            return
        editing = false
        var trimmed = editor.text.trim()
        if (trimmed !== row.text)
            row.saveRequested(trimmed)
    }

    MouseArea {
        id: rowHover
        anchors.fill: card
        enabled: !row.editing
        hoverEnabled: true
        cursorShape: row.readOnly ? Qt.PointingHandCursor : Qt.IBeamCursor
        Accessible.ignored: true
        onPressed: row.forceActiveFocus()
        onClicked: row.readOnly ? row.viewRequested() : row.startEdit()
    }

    Column {
        id: normalContent
        visible: !row.editing && !row.archive
        anchors.left: card.left
        anchors.right: card.right
        anchors.leftMargin: row.contentInset
        anchors.rightMargin: row.readOnly ? 22 : row.actionGutter
        anchors.verticalCenter: card.verticalCenter
        spacing: row.compact ? 3 : 6

        Text {
            width: parent.width
            text: row.highlightedText.length > 0 ? row.highlightedText : row.text
            textFormat: Text.StyledText
            color: row.completed ? row.tokens.muted : row.inkColor
            font.pixelSize: row.tokens.sizeBody + 1
            font.family: row.tokens.fontUi
            font.weight: Font.Bold
            maximumLineCount: row.expanded ? 8 : (row.compact ? 1 : 3)
            elide: row.expanded ? Text.ElideNone : Text.ElideRight
            wrapMode: Text.WrapAnywhere
            lineHeight: 1.15
            renderType: Text.NativeRendering
        }

        Text {
            width: parent.width
            visible: !row.completed
            text: row.repeatLabel.length > 0 ? row.meta + " · 重复：" + row.repeatLabel : row.meta
            color: row.mutedColor
            font.pixelSize: row.tokens.sizeMeta
            font.family: row.tokens.fontUi
            elide: Text.ElideRight
            renderType: Text.NativeRendering
        }

        Row {
            spacing: 7
            width: parent.width
            visible: row.completed

            Rectangle {
                width: 56
                height: 24
                radius: row.tokens.radiusPill
                color: row.tokens.projectAccentSoft
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "已完成"
                    color: row.tokens.projectAccent
                    font.pixelSize: row.tokens.sizeMeta
                    font.weight: Font.DemiBold
                    font.family: row.tokens.fontUi
                    renderType: Text.NativeRendering
                }
            }

            Text {
                width: Math.max(30, parent.width - 64)
                text: row.meta
                color: row.tokens.projectAccent
                font.pixelSize: row.compact ? row.tokens.sizeMeta : row.tokens.sizeBody
                font.weight: Font.DemiBold
                font.family: row.tokens.fontUi
                elide: Text.ElideRight
                renderType: Text.NativeRendering
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Column {
        id: archiveContent
        visible: !row.editing && row.archive
        anchors.left: card.left
        anchors.right: card.right
        anchors.leftMargin: 18
        anchors.rightMargin: row.actionGutter
        anchors.verticalCenter: card.verticalCenter
        spacing: 7

        Row {
            width: parent.width
            spacing: 9

            Rectangle {
                width: 44
                height: 24
                radius: row.tokens.radiusPill
                color: row.tokens.surfaceMuted
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "收纳"
                    color: row.tokens.textSecondary
                    font.pixelSize: row.tokens.sizeMeta
                    font.weight: Font.Bold
                    font.family: row.tokens.fontUi
                    renderType: Text.NativeRendering
                }
            }

            Text {
                width: parent.width - 53
                text: row.text
                color: row.tokens.muted
                font.pixelSize: row.tokens.sizeBody + 1
                font.family: row.tokens.fontUi
                font.weight: Font.DemiBold
                maximumLineCount: row.expanded ? 6 : 2
                elide: row.expanded ? Text.ElideNone : Text.ElideRight
                wrapMode: Text.WrapAnywhere
                lineHeight: 1.18
                renderType: Text.NativeRendering
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Text {
            width: parent.width
            text: row.meta
            color: row.tokens.mutedSoft
            font.pixelSize: row.tokens.sizeMeta
            font.family: row.tokens.fontUi
            maximumLineCount: row.expanded ? 2 : 1
            elide: Text.ElideRight
            renderType: Text.NativeRendering
        }
    }

    TextArea {
        id: editor
        objectName: "noteEditor"
        visible: row.editing
        anchors.left: card.left
        anchors.leftMargin: 14
        anchors.right: card.right
        anchors.rightMargin: 14
        anchors.top: card.top
        anchors.topMargin: 9
        anchors.bottom: card.bottom
        anchors.bottomMargin: 9
        wrapMode: TextEdit.WrapAnywhere
        font.pixelSize: row.tokens.sizeBody + 1
        font.family: row.tokens.fontUi
        color: row.inkColor
        selectedTextColor: row.inkColor
        selectionColor: row.tokens.accentSoft
        activeFocusOnTab: true
        renderType: Text.NativeRendering
        Accessible.role: Accessible.EditableText
        Accessible.name: "编辑便签"
        Accessible.description: "修改便签内容，按 Enter 保存，按 Esc 取消"
        background: Rectangle {
            radius: row.tokens.radiusMd
            color: row.tokens.surfaceMuted
            border.color: editor.activeFocus ? row.tokens.focusRing : row.tokens.border
            border.width: editor.activeFocus ? 2 : 1
        }
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                row.commitEdit()
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                row.editing = false
                event.accepted = true
            }
        }
        onActiveFocusChanged: if (!activeFocus) row.commitEdit()
    }

    Row {
        id: actionRow
        property bool hovering: completeButton.hovered || deleteButton.hovered || viewButton.hovered

        anchors.right: card.right
        anchors.rightMargin: 18
        anchors.verticalCenter: card.verticalCenter
        spacing: 4
        visible: !row.editing
        opacity: row.hovering ? 1 : 0.74
        z: 4
        Behavior on opacity { NumberAnimation { duration: row.tokens.motionMedium; easing.type: Easing.OutCubic } }

        Button {
            id: completeButton
            objectName: "completeNoteButton"
            visible: !row.readOnly && !row.completed
            flat: true
            hoverEnabled: true
            activeFocusOnTab: true
            text: "完成"
            onClicked: row.toggleRequested()
            implicitWidth: 44
            implicitHeight: row.tokens.controlHeight
            Accessible.role: Accessible.Button
            Accessible.name: "完成便签"
            Accessible.description: "将这条便签标记为完成"
            contentItem: Text {
                text: completeButton.text
                color: completeButton.hovered ? row.tokens.projectAccent : row.tokens.textSecondary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: row.tokens.sizeBody
                font.weight: Font.DemiBold
                font.family: row.tokens.fontUi
                renderType: Text.NativeRendering
            }
            background: Rectangle {
                radius: row.tokens.radiusSm
                color: completeButton.pressed ? row.tokens.surfacePressed
                     : completeButton.hovered ? row.tokens.projectAccentSoft
                                              : row.tokens.transparent
                border.width: completeButton.visualFocus ? 2 : 0
                border.color: row.tokens.focusRing
                Behavior on color { ColorAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
            }
            scale: completeButton.pressed ? 0.96 : 1
            Behavior on scale { NumberAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
        }

        Button {
            id: deleteButton
            objectName: "deleteNoteButton"
            visible: !row.readOnly
            flat: true
            hoverEnabled: true
            activeFocusOnTab: true
            text: "删除"
            onClicked: row.deleteRequested()
            implicitWidth: 44
            implicitHeight: row.tokens.controlHeight
            Accessible.role: Accessible.Button
            Accessible.name: "删除便签"
            Accessible.description: "永久删除这条便签"
            contentItem: Text {
                text: deleteButton.text
                color: deleteButton.hovered ? row.tokens.danger : row.tokens.muted
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: row.tokens.sizeBody
                font.weight: Font.DemiBold
                font.family: row.tokens.fontUi
                renderType: Text.NativeRendering
            }
            background: Rectangle {
                radius: row.tokens.radiusSm
                color: deleteButton.pressed || deleteButton.hovered
                     ? row.tokens.dangerSoft : row.tokens.transparent
                border.width: deleteButton.visualFocus ? 2 : 0
                border.color: row.tokens.focusRing
                Behavior on color { ColorAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
            }
            scale: deleteButton.pressed ? 0.96 : 1
            Behavior on scale { NumberAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
        }

        Button {
            id: viewButton
            objectName: "viewNoteButton"
            visible: true
            flat: true
            hoverEnabled: true
            activeFocusOnTab: true
            text: row.readOnly ? "查看" : "详情"
            onClicked: row.viewRequested()
            implicitWidth: 44
            implicitHeight: row.tokens.controlHeight
            Accessible.role: Accessible.Button
            Accessible.name: row.readOnly ? "查看便签详情" : "打开便签详情"
            Accessible.description: row.readOnly ? "查看这条自动收纳便签" : "打开并编辑这条便签"
            contentItem: Text {
                text: viewButton.text
                color: viewButton.hovered ? row.tokens.accent : row.tokens.textSecondary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: row.tokens.sizeBody
                font.weight: Font.DemiBold
                font.family: row.tokens.fontUi
                renderType: Text.NativeRendering
            }
            background: Rectangle {
                radius: row.tokens.radiusSm
                color: viewButton.pressed ? row.tokens.surfacePressed
                     : viewButton.hovered ? row.tokens.accentSoft
                                          : row.tokens.transparent
                border.width: viewButton.visualFocus ? 2 : 0
                border.color: row.tokens.focusRing
                Behavior on color { ColorAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
            }
            scale: viewButton.pressed ? 0.96 : 1
            Behavior on scale { NumberAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
        }
    }
}
