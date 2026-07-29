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
        reduceMotion: root.app.reduceMotion
    }
    readonly property var tokens: root.appTokens

    width: 900
    height: 620
    minimumWidth: 700
    minimumHeight: 520
    visible: true
    title: "ChronoNotes"
    color: root.paperColor
    flags: Qt.Window
    font.family: root.appTokens.fontUi
    font.pixelSize: root.appTokens.sizeBody
    background: Item {}

    property string panel: ""
    property string workspace: "notes"
    property bool searchOpen: false
    property real drawerWidth: panel === "" ? 0 : Math.min(440, Math.max(360, width * 0.42))
    property color paperColor: root.workspace === "projects" ? tokens.paperProject : tokens.paper
    property color cardColor: tokens.card
    property color inkColor: tokens.ink
    property color mutedColor: tokens.muted
    property color accentColor: tokens.accentYellow
    property color blueColor: tokens.accentBlue
    property url pendingImportFile
    property string pendingImportText: ""
    property url pendingWorkspaceFile
    property url pendingRestoreFile
    property var pendingWorkspacePreview: ({})
    property var focusReturnItem: null
    property string recoveryStatus: ""
    property bool recoveryBusy: false
    required property var app
    required property var projectModel
    property var workspaceRecovery: null
    readonly property var projectPanel: projectTreeLoader.item

    Behavior on drawerWidth { NumberAnimation { duration: root.tokens.drawerDuration; easing.type: Easing.OutCubic } }

    function togglePanel(name) {
        if (root.panel === name) {
            root.closeCurrentPanel()
            return
        }
        root.focusReturnItem = root.activeFocusItem
        root.panel = name
        if (name === "settings" && root.workspaceRecovery)
            root.workspaceRecovery.refreshBackups()
        Qt.callLater(drawer.focusInitial)
    }

    function closeCurrentPanel() {
        if (root.panel === "detail")
            root.app.clearSelectedEvent()
        drawer.releaseInputFocus()
        root.panel = ""
        const returnTarget = root.focusReturnItem
        root.focusReturnItem = null
        Qt.callLater(function() {
            if (returnTarget && returnTarget.visible && returnTarget.enabled)
                returnTarget.forceActiveFocus(Qt.TabFocusReason)
        })
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

    function projectSummaryContext() {
        return root.projectPanel ? root.projectPanel.aiContextText : ""
    }

    function projectSummaryEmpty() {
        return !root.projectPanel || !root.projectPanel.aiSummaryAvailable
    }

    function workspacePreviewText(preview) {
        if (!preview || !preview.valid)
            return preview && preview.error ? preview.error : "无法预览该工作区文件。"
        return "将替换当前工作区：\n"
                + "便签 " + preview.currentNoteCount + " → " + preview.noteCount
                + "（变化 " + preview.noteDelta + "）\n"
                + "项目 " + preview.currentProjectCount + " → " + preview.projectCount
                + "（变化 " + preview.projectDelta + "）\n"
                + "摘要历史 " + preview.currentSummaryCount + " → " + preview.summaryCount
                + "\n确认后会先自动保护当前数据，再以一次提交完成恢复。"
    }

    function previewWorkspace(fileUrl, restoreMode) {
        if (!root.workspaceRecovery) {
            root.showToast("恢复中心暂不可用")
            return
        }
        const preview = root.workspaceRecovery.previewImport(fileUrl)
        root.pendingWorkspacePreview = preview
        if (!preview.valid) {
            root.showToast(preview.error || "工作区文件校验失败")
            return
        }
        if (restoreMode)
            root.pendingRestoreFile = fileUrl
        else
            root.pendingWorkspaceFile = fileUrl
        workspaceImportConfirmDialog.restoreMode = restoreMode
        workspaceImportConfirmDialog.open()
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
        }
        function onSelectedEventChanged() {
            if (root.panel === "detail" && !root.app.hasSelectedEvent)
                root.panel = ""
        }
        function onOperationFailed(code, message, recoverable) {
            root.showToast(message)
        }
    }

    Connections {
        target: root.workspaceRecovery
        ignoreUnknownSignals: true

        function onOperationSucceeded(message, path) {
            root.recoveryBusy = false
            root.recoveryStatus = message
            root.showToast(message)
        }

        function onOperationFailed(message) {
            root.recoveryBusy = false
            root.recoveryStatus = ""
            root.showToast(message)
        }

        function onBackupsChanged() {
            root.recoveryBusy = false
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
            showWindowControls: false
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
                    searchActive: root.app.searchActive
                    searchQuery: root.app.searchQuery
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

                Loader {
                    id: projectTreeLoader
                    objectName: "projectTreeLoader"
                    active: root.workspace === "projects"
                    anchors.fill: parent
                    asynchronous: false
                    sourceComponent: Component {
                        ProjectTreePanel {
                            model: root.projectModel
                            inkColor: root.inkColor
                            mutedColor: root.mutedColor
                            accentColor: root.tokens.projectAccent
                            theme: root.appTokens
                            onNoticeRequested: function(message) {
                                root.showToast(message)
                            }
                        }
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
            color: root.tokens.scrim
            visible: opacity > 0
            opacity: root.panel === "" ? 0 : 1
            z: 24

            Behavior on opacity {
                NumberAnimation { duration: root.tokens.drawerDuration; easing.type: Easing.OutCubic }
            }

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
            aiBusy: root.app.aiBusy
            aiState: root.app.aiState
            aiError: root.app.aiError
            hasApiKey: root.app.hasApiKey
            detailText: root.app.selectedEventText
            detailMeta: root.app.selectedEventMeta
            detailRepeat: root.app.selectedEventRepeat
            detailReadOnly: root.app.selectedEventReadOnly
            summaryScope: root.workspace === "projects" ? "projects" : "stickies"
            summaryContextText: root.workspace === "projects" ? root.projectSummaryContext() : ""
            summaryEmpty: root.workspace === "projects" && root.projectSummaryEmpty()
            workspaceSurfaceColor: root.paperColor
            theme: root.appTokens
            reduceMotion: root.app.reduceMotion
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
                drawer.aiResultText = ""
                if (root.workspace === "projects")
                    root.app.summarizeContextAsync(requirement, contextText)
                else
                    root.app.summarizeAsync(requirement)
            }
            onCancelAiRequested: root.app.cancelSummary()
            apiUrl: root.app.apiUrl
            modelName: root.app.modelName
            allowLocalHttp: root.app.allowLocalHttp
            dataDirectory: root.app.dataDirectory
            uiFontFamily: root.app.uiFontFamily
            uiFontSize: root.app.uiFontSize
            uiFontFamilies: root.app.uiFontFamilies
            backupEntries: root.workspaceRecovery ? root.workspaceRecovery.backups : []
            recoveryStatus: root.recoveryStatus
            recoveryError: root.workspaceRecovery ? root.workspaceRecovery.lastError : ""
            recoveryBusy: root.recoveryBusy
            onSaveSettingsRequested: function(url, newKey, model, allowHttp, reduceAnimations,
                                              fontFamily, fontSize) {
                root.app.apiUrl = url
                root.app.modelName = model
                root.app.allowLocalHttp = allowHttp
                root.app.reduceMotion = reduceAnimations
                root.app.uiFontFamily = fontFamily
                root.app.uiFontSize = fontSize
                if (!root.app.saveSettings(newKey))
                    return
                drawer.clearApiKeyDraft()
                root.closeCurrentPanel()
            }
            onClearApiKeyRequested: {
                if (root.app.clearApiKey())
                    root.showToast("API Key 已删除")
            }
            onCreateBackupRequested: {
                if (!root.workspaceRecovery) {
                    root.showToast("恢复中心暂不可用")
                    return
                }
                root.recoveryBusy = true
                root.workspaceRecovery.createBackup()
            }
            onExportWorkspaceRequested: {
                if (!root.workspaceRecovery) {
                    root.showToast("恢复中心暂不可用")
                    return
                }
                root.recoveryBusy = true
                root.workspaceRecovery.createBackup()
            }
            onImportWorkspaceRequested: workspaceImportDialog.open()
            onRestoreBackupRequested: function(backupUrl) {
                if (!backupUrl || backupUrl.toString().length === 0) {
                    root.showToast("请选择一个可恢复备份")
                    return
                }
                root.previewWorkspace(backupUrl, true)
            }
            onExportDiagnosticsRequested: diagnosticsExportDialog.open()
            onOpenDataDirectoryRequested: {
                const normalized = root.app.dataDirectory.replace(/\\/g, "/")
                Qt.openUrlExternally("file:///" + normalized)
            }
            onRefreshBackupsRequested: {
                if (root.workspaceRecovery) {
                    root.recoveryBusy = true
                    root.workspaceRecovery.refreshBackups()
                    root.recoveryBusy = false
                }
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

    FileDialog {
        id: workspaceImportDialog
        title: "选择 ChronoNotes 工作区备份"
        fileMode: FileDialog.OpenFile
        nameFilters: ["ChronoNotes 工作区 (*.chrononotes)", "所有文件 (*)"]
        onAccepted: root.previewWorkspace(selectedFile, false)
    }

    Dialog {
        id: workspaceImportConfirmDialog
        property bool restoreMode: false

        modal: true
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: Math.min(root.width - 64, 460)
        title: restoreMode ? "确认恢复备份" : "确认导入工作区"
        standardButtons: Dialog.Ok | Dialog.Cancel
        closePolicy: Popup.CloseOnEscape
        onAccepted: {
            if (!root.workspaceRecovery)
                return
            root.recoveryBusy = true
            const ok = restoreMode
                       ? root.workspaceRecovery.restoreBackup(root.pendingRestoreFile)
                       : root.workspaceRecovery.importWorkspace(root.pendingWorkspaceFile)
            if (!ok)
                root.recoveryBusy = false
        }

        contentItem: Text {
            width: workspaceImportConfirmDialog.availableWidth
            text: root.workspacePreviewText(root.pendingWorkspacePreview)
            color: root.inkColor
            wrapMode: Text.WordWrap
            font.pixelSize: root.tokens.sizeBody
            font.family: root.tokens.fontUi
            renderType: Text.NativeRendering
            Accessible.role: Accessible.StaticText
            Accessible.name: workspaceImportConfirmDialog.title
            Accessible.description: text
        }
    }

    FileDialog {
        id: diagnosticsExportDialog
        title: "导出脱敏诊断"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "zip"
        selectedFile: "chrononotes-diagnostics.zip"
        nameFilters: ["ZIP 压缩包 (*.zip)", "所有文件 (*)"]
        onAccepted: {
            if (!root.workspaceRecovery) {
                root.showToast("恢复中心暂不可用")
                return
            }
            root.recoveryBusy = true
            const path = root.workspaceRecovery.exportDiagnostics(selectedFile)
            if (!path || path.length === 0)
                root.recoveryBusy = false
        }
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
        color: root.tokens.toastSurface
        opacity: 0
        z: 80

        Text {
            id: toastText
            anchors.centerIn: parent
            color: root.tokens.toastText
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
