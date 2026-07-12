pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Window

ApplicationWindow {
    id: root

    readonly property ChronoTokens appTokens: ChronoTokens {
        fontUi: root.app.uiFontFamily
        baseFontSize: root.app.uiFontSize
    }
    readonly property var tokens: root.appTokens

    width: 900
    height: 620
    minimumWidth: 700
    minimumHeight: 520
    visible: true
    title: "ChronoNotes"
    color: root.paperColor
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    font.family: root.appTokens.fontUi
    font.pixelSize: root.appTokens.sizeBody
    background: Item {}

    property string panel: ""
    property string workspace: "notes"
    property bool aiBusy: false
    property bool searchOpen: false
    property real drawerWidth: panel === "" ? 0 : Math.min(320, Math.max(300, width * 0.34))
    property color paperColor: root.workspace === "projects" ? tokens.paperProject : tokens.paper
    property color cardColor: tokens.card
    property color inkColor: tokens.ink
    property color mutedColor: tokens.muted
    property color accentColor: tokens.accentYellow
    property color blueColor: tokens.accentBlue
    property url pendingImportFile
    property string pendingImportText: ""
    required property var app
    required property var projectModel

    Behavior on drawerWidth { NumberAnimation { duration: root.tokens.drawerDuration; easing.type: Easing.OutCubic } }

    function togglePanel(name) {
        root.panel = root.panel === name ? "" : name
    }

    function closeCurrentPanel() {
        if (root.panel === "detail")
            root.app.clearSelectedEvent()
        root.panel = ""
    }

    function switchWorkspace(name) {
        if (root.workspace === name)
            return
        root.workspace = name
        root.searchOpen = false
        root.app.searchQuery = ""
        root.app.searchCompletionFilter = -1
        if (root.panel === "detail")
            root.closeCurrentPanel()
    }

    function focusComposer() {
        root.switchWorkspace("notes")
        root.closeCurrentPanel()
        root.app.searchQuery = ""
        root.searchOpen = false
        Qt.callLater(composer.forceComposerFocus)
    }

    function handleEscape() {
        if (drawer.inputActiveFocus) {
            drawer.releaseInputFocus()
            return
        }
        if (root.panel !== "") {
            root.closeCurrentPanel()
            return
        }
        if (root.workspace === "notes" && (root.searchOpen || root.app.searchActive)) {
            root.app.searchQuery = ""
            root.app.searchCompletionFilter = -1
            root.searchOpen = false
            return
        }
        if (composer.inputActiveFocus)
            composer.releaseComposerFocus()
    }

    function showToast(message) {
        toastText.text = message
        toastTimer.restart()
        toast.opacity = 1
        toast.y = root.height - 64
    }

    function defaultExportName(suffix) {
        return "stickies-export." + suffix
    }

    Connections {
        target: root.app
        function onNoticeChanged() {
            if (root.app.notice.length > 0)
                root.showToast(root.app.notice)
        }
        function onSummaryReady(result) {
            drawer.aiResultText = result
            root.aiBusy = false
        }
        function onSelectedEventChanged() {
            if (root.panel === "detail" && !root.app.hasSelectedEvent)
                root.panel = ""
        }
    }

    Shortcut {
        sequence: StandardKey.Undo
        enabled: root.app.canUndo
        onActivated: root.app.undoLastAction()
    }

    Shortcut {
        sequence: StandardKey.Find
        enabled: root.workspace === "notes"
        onActivated: {
            root.searchOpen = true
            Qt.callLater(searchBar.forceSearchFocus)
        }
    }

    Shortcut {
        sequence: "Ctrl+N"
        onActivated: root.focusComposer()
    }

    Shortcut {
        sequence: "Esc"
        enabled: root.panel !== "" || root.searchOpen || root.app.searchActive || composer.inputActiveFocus
        onActivated: root.handleEscape()
    }

    component ResizeHandle: MouseArea {
        required property int edges
        property int edgeSize: 8

        acceptedButtons: Qt.LeftButton
        z: 60
        onPressed: root.startSystemResize(edges)
    }

    Rectangle {
        id: paper
        anchors.fill: parent
        radius: 0
        color: root.paperColor
        clip: false
        antialiasing: true
        layer.enabled: false

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 52
            z: -1
            color: root.workspace === "projects" ? root.tokens.mintSoft : root.tokens.paperSoft
            opacity: 0.62
        }

        AppTitleBar {
            id: titlebar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            titleText: ""
            activePanel: root.panel
            workspace: root.workspace
            theme: root.appTokens
            uiFontFamily: root.app.uiFontFamily
            uiFontSize: root.app.uiFontSize
            windowVisibility: root.visibility
            onMoveRequested: root.startSystemMove()
            onWorkspaceRequested: function(name) {
                root.switchWorkspace(name)
            }
            onAiClicked: root.togglePanel("ai")
            onSettingsClicked: root.togglePanel("settings")
            onMinimizeClicked: root.showMinimized()
            onMaximizeRestoreClicked: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
            onCloseClicked: root.close()
        }

        Item {
            id: mainArea
            anchors.left: parent.left
            anchors.top: titlebar.bottom
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            clip: true

            Item {
                id: contentRoot
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                anchors.topMargin: 12
                anchors.bottomMargin: 18

                StageTabs {
                    id: stageTabs
                    visible: root.workspace === "notes"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    stage: root.app.stage
                    allCompleted: root.app.allCompleted
                    theme: root.appTokens
                    uiFontFamily: root.app.uiFontFamily
                    uiFontSize: root.app.uiFontSize
                    onStageRequested: function(stage) {
                        root.app.stage = stage
                        root.app.searchQuery = ""
                        root.app.searchCompletionFilter = -1
                        root.searchOpen = false
                        root.panel = ""
                        noteList.resetScroll()
                    }
                    onToggleAllRequested: root.app.toggleAll()
                }

                SearchBar {
                    id: searchBar
                    visible: root.workspace === "notes"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: stageTabs.bottom
                    anchors.topMargin: open ? 8 : 0
                    open: root.searchOpen || root.app.searchActive
                    query: root.app.searchQuery
                    completionFilter: root.app.searchCompletionFilter
                    inkColor: root.inkColor
                    mutedColor: root.mutedColor
                    theme: root.appTokens
                    uiFontFamily: root.app.uiFontFamily
                    uiFontSize: root.app.uiFontSize
                    onQueryEdited: function(text) {
                        root.app.searchQuery = text
                        root.searchOpen = text.length > 0 || root.searchOpen
                    }
                    onCompletionFilterRequested: function(value) {
                        root.app.searchCompletionFilter = value
                        root.searchOpen = true
                    }
                    onCloseRequested: {
                        root.app.searchQuery = ""
                        root.app.searchCompletionFilter = -1
                        root.searchOpen = false
                    }
                }

                EventComposer {
                    id: composer
                    visible: root.workspace === "notes"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: searchBar.bottom
                    anchors.topMargin: searchBar.open ? 8 : 10
                    stage: root.app.stage
                    inkColor: root.inkColor
                    theme: root.appTokens
                    uiFontFamily: root.app.uiFontFamily
                    uiFontSize: root.app.uiFontSize
                    onEmptySubmitted: root.showToast("请输入便签内容")
                    onAddRequested: function(text) {
                        root.app.addEvent(text)
                    }
                }

                NoteListPanel {
                    id: noteList
                    visible: root.workspace === "notes"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: composer.bottom
                    anchors.bottom: parent.bottom
                    anchors.topMargin: 8
                    model: root.app
                    totalCount: root.app.totalCount
                    completedCount: root.app.completedCount
                    hasVisibleRows: root.app.hasVisibleRows
                    stageLabel: root.app.stageLabel
                    overlayOpen: root.panel !== ""
                    blueColor: root.blueColor
                    accentColor: root.accentColor
                    cardColor: root.cardColor
                    inkColor: root.inkColor
                    mutedColor: root.mutedColor
                    theme: root.appTokens
                    uiFontFamily: root.app.uiFontFamily
                    uiFontSize: root.app.uiFontSize
                    onToggleRequested: function(eventId) {
                        root.app.toggleEvent(eventId)
                    }
                    onDeleteRequested: function(eventId) {
                        root.app.deleteEvent(eventId)
                    }
                    onSaveRequested: function(eventId, newText) {
                        if (newText.trim().length === 0) {
                            root.showToast("便签内容不能为空")
                            return
                        }
                        root.app.updateEvent(eventId, newText)
                    }
                    onViewRequested: function(eventId, readOnly) {
                        root.app.selectEvent(eventId, readOnly)
                        root.panel = "detail"
                    }
                }

                ProjectTreePanel {
                    id: projectTree
                    visible: root.workspace === "projects"
                    anchors.fill: parent
                    model: root.projectModel
                    inkColor: root.inkColor
                    mutedColor: root.mutedColor
                    accentColor: tokens.accentMint
                    theme: root.appTokens
                    uiFontFamily: root.app.uiFontFamily
                    uiFontSize: root.app.uiFontSize
                    onNoticeRequested: function(message) {
                        root.showToast(message)
                    }
                }
            }
        }

        Rectangle {
            id: drawerScrim
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titlebar.bottom
            anchors.bottom: parent.bottom
            color: "#1a071426"
            visible: opacity > 0
            opacity: root.panel === "" ? 0 : 1
            z: 24

            Behavior on opacity { NumberAnimation { duration: root.tokens.drawerDuration; easing.type: Easing.OutCubic } }

            MouseArea {
                anchors.fill: parent
                enabled: drawerScrim.opacity > 0
                onClicked: root.closeCurrentPanel()
            }
        }

        OverlayPanel {
            id: drawer
            anchors.right: parent.right
            anchors.top: titlebar.bottom
            anchors.bottom: parent.bottom
            width: root.drawerWidth
            visible: width > 1
            z: 32
            panel: root.panel
            aiBusy: root.aiBusy
            detailText: root.app.selectedEventText
            detailMeta: root.app.selectedEventMeta
            detailRepeat: root.app.selectedEventRepeat
            detailReadOnly: root.app.selectedEventReadOnly
            summaryScope: root.workspace === "projects" ? "projects" : "stickies"
            summaryContextText: root.workspace === "projects" ? projectTree.aiContextText : ""
            summaryEmpty: root.workspace === "projects" && !projectTree.aiSummaryAvailable
            workspaceSurfaceColor: root.paperColor
            theme: root.appTokens
            onCloseRequested: {
                root.closeCurrentPanel()
            }
            onSaveDetailRequested: function(text) {
                if (text.trim().length === 0) {
                    root.showToast("便签内容不能为空")
                    return
                }
                root.app.saveSelectedEvent(text)
            }
            onRepeatDetailRequested: function(repeat) {
                root.app.setEventRepeat(root.app.selectedEventId, repeat)
            }
            onRunAiRequested: function(requirement, contextText) {
                root.aiBusy = true
                drawer.aiResultText = ""
                if (root.workspace === "projects")
                    root.app.summarizeContextAsync(requirement, contextText)
                else
                    root.app.summarizeAsync(requirement)
            }
            apiUrl: root.app.apiUrl
            apiKey: root.app.apiKey
            modelName: root.app.modelName
            uiFontFamily: root.app.uiFontFamily
            uiFontSize: root.app.uiFontSize
            uiFontFamilies: root.app.uiFontFamilies
            onSaveSettingsRequested: function(url, key, model, fontFamily, fontSize) {
                root.app.apiUrl = url
                root.app.apiKey = key
                root.app.modelName = model
                root.app.uiFontFamily = fontFamily
                root.app.uiFontSize = fontSize
                root.app.saveConfig()
                root.panel = ""
            }
            onClearCompletedRequested: root.app.clearCompletedCurrent()
            onClearCompletedAllRequested: root.app.clearCompletedAll()
            onClearCurrentRequested: root.app.clearCurrentStage()
            onClearAllRequested: root.app.clearAllNotes()
            onExportJsonRequested: exportJsonDialog.open()
            onImportJsonRequested: importJsonDialog.open()
            onExportMarkdownRequested: exportMarkdownDialog.open()
        }
    }

    FileDialog {
        id: exportJsonDialog
        title: "选择便签 JSON 导出位置"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        selectedFile: root.defaultExportName("json")
        nameFilters: ["JSON 文件 (*.json)", "所有文件 (*)"]
        onAccepted: root.app.exportJsonToFile(selectedFile)
    }

    FileDialog {
        id: importJsonDialog
        title: "选择要导入的便签 JSON 文件"
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON 文件 (*.json)", "所有文件 (*)"]
        onAccepted: {
            const count = root.app.previewImportJsonEventCount(selectedFile)
            if (count < 0)
                return
            root.pendingImportFile = selectedFile
            root.pendingImportText = "将导入 " + count + " 条便签事件，并覆盖当前便签数据。项目树不会被修改。"
            importConfirmDialog.open()
        }
    }

    Dialog {
        id: importConfirmDialog
        modal: true
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: Math.min(root.width - 64, 360)
        title: "确认导入"
        standardButtons: Dialog.Ok | Dialog.Cancel
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onAccepted: root.app.importJsonFromFile(root.pendingImportFile)

        contentItem: Text {
            width: importConfirmDialog.availableWidth
            text: root.pendingImportText
            color: root.inkColor
            wrapMode: Text.WordWrap
            font.pixelSize: root.tokens.sizeBody + 1
            font.family: root.tokens.fontUi
            renderType: Text.NativeRendering
        }
    }

    FileDialog {
        id: exportMarkdownDialog
        title: "选择便签 Markdown 导出位置"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "md"
        selectedFile: root.defaultExportName("md")
        nameFilters: ["Markdown 文件 (*.md)", "所有文件 (*)"]
        onAccepted: root.app.exportMarkdownToFile(selectedFile)
    }

    ResizeHandle {
        edges: Qt.LeftEdge
        width: edgeSize
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeHorCursor
    }
    ResizeHandle {
        edges: Qt.RightEdge
        width: edgeSize
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeHorCursor
    }
    ResizeHandle {
        edges: Qt.TopEdge
        height: edgeSize
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeVerCursor
    }
    ResizeHandle {
        edges: Qt.BottomEdge
        height: edgeSize
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeVerCursor
    }
    ResizeHandle {
        edges: Qt.LeftEdge | Qt.TopEdge
        width: edgeSize * 2
        height: edgeSize * 2
        anchors.left: parent.left
        anchors.top: parent.top
        cursorShape: Qt.SizeFDiagCursor
    }
    ResizeHandle {
        edges: Qt.RightEdge | Qt.TopEdge
        width: edgeSize * 2
        height: edgeSize * 2
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeBDiagCursor
    }
    ResizeHandle {
        edges: Qt.LeftEdge | Qt.BottomEdge
        width: edgeSize * 2
        height: edgeSize * 2
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeBDiagCursor
    }
    ResizeHandle {
        edges: Qt.RightEdge | Qt.BottomEdge
        width: edgeSize * 2
        height: edgeSize * 2
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeFDiagCursor
    }

    Rectangle {
        id: toast
        width: Math.min(parent.width - 64, toastText.implicitWidth + 34)
        height: 38
        radius: 19
        x: (parent.width - width) / 2
        y: parent.height - 58
        color: "#1f2937"
        opacity: 0
        z: 80

        Text {
            id: toastText
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: root.tokens.sizeBody + 1
            font.family: root.tokens.fontUi
            renderType: Text.NativeRendering
        }

        Behavior on opacity { NumberAnimation { duration: root.tokens.motionMedium; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: root.tokens.motionMedium; easing.type: Easing.OutCubic } }

        Timer {
            id: toastTimer
            interval: 1800
            onTriggered: {
                toast.opacity = 0
                toast.y = root.height - 54
            }
        }
    }
}
