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
    property int actionGutter: archive ? 94 : 150

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
    color: "transparent"
    visible: !collapsedBySection
    opacity: completed || archive ? 0.9 : 1
    border.width: 0
    antialiasing: true
    clip: true

    Behavior on height { NumberAnimation { duration: row.tokens.motionSlow; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: row.tokens.motionMedium; easing.type: Easing.OutCubic } }
    Behavior on y { NumberAnimation { duration: row.tokens.motionSlow; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: row.archive ? row.tokens.radiusMd : row.tokens.radiusMd
        color: row.archive ? "#99fffbea" : row.completed ? "#99fffbea" : row.cardColor
        border.width: 1
        border.color: row.hovering ? "#55f4bf30" : row.tokens.lineSoft
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
        color: row.archive ? row.tokens.mutedSoft : row.completed ? row.blueColor : row.tokens.accentYellow
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
            font.pixelSize: row.compact ? row.tokens.sizeBody + 1 : row.tokens.sizeTitle + 1
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
                width: 18
                height: 18
                radius: 7
                color: row.tokens.accentBlueSoft
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    color: row.blueColor
                    font.pixelSize: row.tokens.sizeBody + 1
                    font.weight: Font.Bold
                    font.family: row.tokens.fontUi
                    renderType: Text.NativeRendering
                }
            }

            Text {
                width: Math.max(30, parent.width - 26)
                text: row.meta
                color: row.blueColor
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
                width: 22
                height: 22
                radius: 8
                color: row.tokens.lineSoft
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "收"
                    color: row.tokens.muted
                    font.pixelSize: row.tokens.sizeMeta
                    font.weight: Font.Bold
                    font.family: row.tokens.fontUi
                    renderType: Text.NativeRendering
                }
            }

            Text {
                width: parent.width - 31
                text: row.text
                color: row.tokens.muted
                font.pixelSize: row.compact ? row.tokens.sizeBody + 1 : row.tokens.sizeTitle + 1
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
        font.pixelSize: row.tokens.sizeTitle + 1
        font.family: row.tokens.fontUi
        color: row.inkColor
        selectedTextColor: row.inkColor
        selectionColor: row.tokens.accentYellowSoft
        renderType: Text.NativeRendering
        background: Rectangle {
            radius: row.tokens.radiusMd
            color: row.tokens.cardQuiet
            border.color: "#66f4bf30"
            border.width: 1
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
        spacing: 8
        visible: !row.editing
        opacity: row.hovering ? 1 : 0.74
        z: 4
        Behavior on opacity { NumberAnimation { duration: row.tokens.motionMedium; easing.type: Easing.OutCubic } }

        Button {
            id: completeButton
            visible: !row.readOnly && !row.completed
            flat: true
            hoverEnabled: true
            text: "完成"
            onClicked: row.toggleRequested()
            implicitWidth: 42
            implicitHeight: 28
            contentItem: Text {
                text: completeButton.text
                color: completeButton.hovered ? row.blueColor : row.tokens.muted
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: row.tokens.sizeBody
                font.weight: Font.DemiBold
                font.family: row.tokens.fontUi
                renderType: Text.NativeRendering
            }
            background: Rectangle {
                radius: 12
                color: completeButton.hovered ? row.tokens.accentBlueSoft : "#00ffffff"
                Behavior on color { ColorAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
            }
            scale: completeButton.pressed ? 0.94 : completeButton.hovered ? 1.05 : 1
            Behavior on scale { NumberAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
        }

        Button {
            id: deleteButton
            visible: !row.readOnly
            flat: true
            hoverEnabled: true
            text: "删除"
            onClicked: row.deleteRequested()
            implicitWidth: 42
            implicitHeight: 28
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
                radius: 12
                color: deleteButton.hovered ? row.tokens.dangerSoft : "#00ffffff"
                Behavior on color { ColorAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
            }
            scale: deleteButton.pressed ? 0.94 : deleteButton.hovered ? 1.05 : 1
            Behavior on scale { NumberAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
        }

        Button {
            id: viewButton
            visible: true
            flat: true
            hoverEnabled: true
            text: row.readOnly ? "查看" : "详情"
            onClicked: row.viewRequested()
            implicitWidth: 44
            implicitHeight: 28
            contentItem: Text {
                text: viewButton.text
                color: viewButton.hovered ? row.blueColor : row.tokens.muted
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: row.tokens.sizeBody
                font.weight: Font.DemiBold
                font.family: row.tokens.fontUi
                renderType: Text.NativeRendering
            }
            background: Rectangle {
                radius: 12
                color: viewButton.hovered ? row.tokens.accentBlueSoft : "#00ffffff"
                Behavior on color { ColorAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
            }
            scale: viewButton.pressed ? 0.94 : viewButton.hovered ? 1.05 : 1
            Behavior on scale { NumberAnimation { duration: row.tokens.motionFast; easing.type: Easing.OutCubic } }
        }
    }
}
