pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Rectangle {
    id: panelRoot
    objectName: "overlayPanel"

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: panelRoot.uiFontFamily
        baseFontSize: panelRoot.uiFontSize
        reduceMotion: panelRoot.reduceMotion
    }
    readonly property var tokens: panelRoot.theme ? panelRoot.theme : panelRoot.fallbackTokens

    property string panel: ""
    property bool aiBusy: false
    property string aiState: "idle"
    property string aiError: ""
    property bool hasApiKey: false
    property string aiResultText: ""
    property string summaryScope: "stickies"
    property string summaryContextText: ""
    property bool summaryEmpty: false
    property color workspaceSurfaceColor: tokens.paper
    property string detailText: ""
    property string detailMeta: ""
    property string detailRepeat: ""
    property bool detailReadOnly: false
    property string apiUrl: ""
    property string modelName: ""
    property bool allowLocalHttp: false
    property bool reduceMotion: false
    property string dataDirectory: ""
    property var backupEntries: []
    property string recoveryStatus: ""
    property string recoveryError: ""
    property bool recoveryBusy: false
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 14
    property var uiFontFamilies: ["Microsoft YaHei UI"]
    readonly property var loadedPage: pageLoader.item
    readonly property bool inputActiveFocus: loadedPage
                                             ? Boolean(loadedPage.inputActiveFocus)
                                             : false

    signal closeRequested()
    signal runAiRequested(string requirement, string contextText)
    signal cancelAiRequested()
    signal saveDetailRequested(string text)
    signal repeatDetailRequested(string repeat)
    signal saveSettingsRequested(string url, string newKey, string model, bool allowHttp,
                                 bool reduceAnimations, string fontFamily, int fontSize)
    signal clearApiKeyRequested()
    signal createBackupRequested()
    signal exportWorkspaceRequested()
    signal importWorkspaceRequested()
    signal restoreBackupRequested(var backupUrl)
    signal exportDiagnosticsRequested()
    signal openDataDirectoryRequested()
    signal refreshBackupsRequested()
    signal clearCompletedRequested()
    signal clearCompletedAllRequested()
    signal clearCurrentRequested()
    signal clearAllRequested()
    signal exportJsonRequested()
    signal importJsonRequested()
    signal exportMarkdownRequested()

    color: tokens.drawerPaper
    clip: true
    antialiasing: true
    border.width: 1
    border.color: tokens.border
    opacity: panelRoot.width > 8 ? 1 : 0
    Accessible.role: Accessible.Dialog
    Accessible.name: panel === "ai" ? "智能摘要"
                     : panel === "detail" ? "便签详情"
                     : panel === "settings" ? "设置与恢复中心"
                     : "侧边面板"

    Behavior on opacity {
        NumberAnimation { duration: panelRoot.tokens.motionMedium; easing.type: Easing.OutCubic }
    }

    function releaseInputFocus() {
        if (loadedPage && typeof loadedPage.releaseInputFocus === "function")
            loadedPage.releaseInputFocus()
    }

    function focusInitial() {
        if (loadedPage && typeof loadedPage.focusInitial === "function")
            loadedPage.focusInitial()
        else
            closeButton.forceActiveFocus(Qt.TabFocusReason)
    }

    function clearApiKeyDraft() {
        if (panelRoot.panel === "settings" && loadedPage &&
                typeof loadedPage.clearApiKeyDraft === "function")
            loadedPage.clearApiKeyDraft()
    }

    onPanelChanged: {
        if (loadedPage && loadedPage.clearAllArmed !== undefined)
            loadedPage.clearAllArmed = false
    }

    Button {
        id: closeButton
        objectName: "overlayCloseButton"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: panelRoot.tokens.space3
        width: panelRoot.tokens.controlHeight
        height: panelRoot.tokens.controlHeight
        hoverEnabled: true
        activeFocusOnTab: true
        z: 5
        onClicked: panelRoot.closeRequested()
        Accessible.role: Accessible.Button
        Accessible.name: "关闭侧边面板"
        Accessible.description: "关闭当前面板并返回之前的控件"

        contentItem: Image {
            source: "qrc:/assets/icons/close.svg"
            width: 20
            height: 20
            sourceSize.width: 40
            sourceSize.height: 40
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        background: Rectangle {
            radius: panelRoot.tokens.radiusSm
            color: closeButton.pressed ? panelRoot.tokens.surfacePressed
                 : closeButton.hovered ? panelRoot.tokens.dangerSoft
                                       : panelRoot.tokens.transparent
            border.width: closeButton.visualFocus ? 2 : 0
            border.color: panelRoot.tokens.focusRing
            Behavior on color {
                ColorAnimation { duration: panelRoot.tokens.motionFast; easing.type: Easing.OutCubic }
            }
        }
    }

    Loader {
        id: pageLoader
        objectName: "overlayPageLoader"
        anchors.fill: parent
        anchors.leftMargin: panelRoot.tokens.space4
        anchors.rightMargin: panelRoot.tokens.space4
        anchors.topMargin: panelRoot.tokens.controlHeight + panelRoot.tokens.space2
        anchors.bottomMargin: panelRoot.tokens.space4
        active: panelRoot.panel === "ai" || panelRoot.panel === "detail" ||
                panelRoot.panel === "settings"
        asynchronous: false
        sourceComponent: panelRoot.panel === "ai" ? aiPage
                         : panelRoot.panel === "detail" ? detailPage
                         : panelRoot.panel === "settings" ? settingsPage
                         : null
    }

    Component {
        id: aiPage

        AiSummaryPanel {
            aiBusy: panelRoot.aiBusy
            aiState: panelRoot.aiState
            aiError: panelRoot.aiError
            hasApiKey: panelRoot.hasApiKey
            aiResultText: panelRoot.aiResultText
            summaryScope: panelRoot.summaryScope
            summaryContextText: panelRoot.summaryContextText
            summaryEmpty: panelRoot.summaryEmpty
            surfaceColor: panelRoot.tokens.drawerPaper
            theme: panelRoot.tokens
            onRunRequested: function(requirement, contextText) {
                panelRoot.runAiRequested(requirement, contextText)
            }
            onCancelRequested: panelRoot.cancelAiRequested()
        }
    }

    Component {
        id: detailPage

        DetailPanel {
            eventText: panelRoot.detailText
            eventMeta: panelRoot.detailMeta
            eventRepeat: panelRoot.detailRepeat
            readOnly: panelRoot.detailReadOnly
            theme: panelRoot.tokens
            onSaveRequested: function(text) {
                panelRoot.saveDetailRequested(text)
            }
            onRepeatRequested: function(repeat) {
                panelRoot.repeatDetailRequested(repeat)
            }
        }
    }

    Component {
        id: settingsPage

        SettingsPanel {
            theme: panelRoot.tokens
            apiUrl: panelRoot.apiUrl
            modelName: panelRoot.modelName
            hasApiKey: panelRoot.hasApiKey
            allowLocalHttp: panelRoot.allowLocalHttp
            reduceMotion: panelRoot.reduceMotion
            dataDirectory: panelRoot.dataDirectory
            uiFontFamilies: panelRoot.uiFontFamilies
            backupEntries: panelRoot.backupEntries
            recoveryStatus: panelRoot.recoveryStatus
            recoveryError: panelRoot.recoveryError
            recoveryBusy: panelRoot.recoveryBusy
            surfaceColor: panelRoot.tokens.drawerPaper
            onSaveRequested: function(url, newKey, model, allowHttp, reduceAnimations,
                                      fontFamily, fontSize) {
                panelRoot.saveSettingsRequested(url, newKey, model, allowHttp,
                                                reduceAnimations, fontFamily, fontSize)
            }
            onClearApiKeyRequested: panelRoot.clearApiKeyRequested()
            onCreateBackupRequested: panelRoot.createBackupRequested()
            onExportWorkspaceRequested: panelRoot.exportWorkspaceRequested()
            onImportWorkspaceRequested: panelRoot.importWorkspaceRequested()
            onRestoreBackupRequested: function(backupUrl) {
                panelRoot.restoreBackupRequested(backupUrl)
            }
            onExportDiagnosticsRequested: panelRoot.exportDiagnosticsRequested()
            onOpenDataDirectoryRequested: panelRoot.openDataDirectoryRequested()
            onRefreshBackupsRequested: panelRoot.refreshBackupsRequested()
            onClearCompletedRequested: panelRoot.clearCompletedRequested()
            onClearCompletedAllRequested: panelRoot.clearCompletedAllRequested()
            onClearCurrentRequested: panelRoot.clearCurrentRequested()
            onClearAllRequested: panelRoot.clearAllRequested()
            onExportJsonRequested: panelRoot.exportJsonRequested()
            onImportJsonRequested: panelRoot.importJsonRequested()
            onExportMarkdownRequested: panelRoot.exportMarkdownRequested()
        }
    }
}
