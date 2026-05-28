pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Window

ApplicationWindow {
    id: root

    width: 900
    height: 620
    minimumWidth: 700
    minimumHeight: 520
    visible: true
    title: "便签"
    color: root.paperColor
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    background: Item {}

    property string panel: ""
    property string workspace: "notes"
    property bool aiBusy: false
    property bool searchOpen: false
    property real drawerWidth: panel === "" ? 0 : Math.min(panel === "detail" ? 420 : 318, Math.max(panel === "detail" ? 330 : 276, width * (panel === "detail" ? 0.42 : 0.34)))
    property color paperColor: root.workspace === "projects" ? "#e9f6f2" : "#fff5af"
    property color cardColor: "#fffef7"
    property color inkColor: "#071426"
    property color mutedColor: "#64748b"
    property color accentColor: "#f4bf30"
    property color blueColor: "#2d68c7"
    property url pendingImportFile
    property string pendingImportText: ""
    required property var app
    required property var projectModel

    Behavior on drawerWidth { NumberAnimation { duration: 460; easing.type: Easing.OutCubic } }

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
        return "notes-export." + suffix
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
            anchors.fill: parent
            z: -1
            gradient: Gradient {
                GradientStop { position: 0.0; color: root.workspace === "projects" ? "#f7fffc" : "#fffdf2" }
                GradientStop { position: 0.14; color: root.workspace === "projects" ? "#f7fffc" : "#fffdf2" }
                GradientStop { position: 0.15; color: root.workspace === "projects" ? "#e0f2ed" : "#fff5af" }
                GradientStop { position: 1.0; color: root.workspace === "projects" ? "#cce7e8" : "#f8ed9c" }
            }
        }

        AppTitleBar {
            id: titlebar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            titleText: root.workspace === "projects" ? "项目树 · 构建与分化" : "便签 · " + root.app.stageLabel + " " + root.app.dateKey
            activePanel: root.panel
            workspace: root.workspace
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
            anchors.right: drawer.left
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
                    onEmptySubmitted: root.showToast("请输入事件内容")
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
                    onToggleRequested: function(eventId) {
                        root.app.toggleEvent(eventId)
                    }
                    onDeleteRequested: function(eventId) {
                        root.app.deleteEvent(eventId)
                    }
                    onSaveRequested: function(eventId, newText) {
                        if (newText.trim().length === 0) {
                            root.showToast("事件内容不能为空")
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
                    onNoticeRequested: function(message) {
                        root.showToast(message)
                    }
                }
            }
        }

        OverlayPanel {
            id: drawer
            anchors.right: parent.right
            anchors.top: titlebar.bottom
            anchors.bottom: parent.bottom
            width: root.drawerWidth
            visible: width > 1
            panel: root.panel
            aiBusy: root.aiBusy
            detailText: root.app.selectedEventText
            detailMeta: root.app.selectedEventMeta
            detailRepeat: root.app.selectedEventRepeat
            detailReadOnly: root.app.selectedEventReadOnly
            onCloseRequested: {
                root.closeCurrentPanel()
            }
            onSaveDetailRequested: function(text) {
                if (text.trim().length === 0) {
                    root.showToast("事件内容不能为空")
                    return
                }
                root.app.saveSelectedEvent(text)
            }
            onRepeatDetailRequested: function(repeat) {
                root.app.setEventRepeat(root.app.selectedEventId, repeat)
            }
            onRunAiRequested: function(requirement) {
                root.aiBusy = true
                drawer.aiResultText = ""
                root.app.summarizeAsync(requirement)
            }
            apiUrl: root.app.apiUrl
            apiKey: root.app.apiKey
            modelName: root.app.modelName
            onSaveSettingsRequested: function(url, key, model) {
                root.app.apiUrl = url
                root.app.apiKey = key
                root.app.modelName = model
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
        title: "选择 JSON 导出位置"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        selectedFile: root.defaultExportName("json")
        nameFilters: ["JSON 文件 (*.json)", "所有文件 (*)"]
        onAccepted: root.app.exportJsonToFile(selectedFile)
    }

    FileDialog {
        id: importJsonDialog
        title: "选择要导入的 JSON 文件"
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON 文件 (*.json)", "所有文件 (*)"]
        onAccepted: {
            const count = root.app.previewImportJsonEventCount(selectedFile)
            if (count < 0)
                return
            root.pendingImportFile = selectedFile
            root.pendingImportText = "将导入 " + count + " 条事件，并覆盖当前数据。"
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
            font.pixelSize: 13
            font.family: "Microsoft YaHei UI"
            renderType: Text.NativeRendering
        }
    }

    FileDialog {
        id: exportMarkdownDialog
        title: "选择 Markdown 导出位置"
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
            font.pixelSize: 13
            font.family: "Microsoft YaHei UI"
            renderType: Text.NativeRendering
        }

        Behavior on opacity { NumberAnimation { duration: 160 } }
        Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

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
