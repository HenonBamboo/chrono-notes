import QtQuick
import QtQuick.Controls

Rectangle {
    id: panelRoot

    readonly property ChronoTokens tokens: ChronoTokens {}

    property string panel: ""
    property bool aiBusy: false
    property string aiResultText: ""
    property string summaryScope: "stickies"
    property string summaryContextText: ""
    property bool summaryEmpty: false
    property color workspaceSurfaceColor: tokens.paper
    property string detailText: ""
    property string detailMeta: ""
    property string detailRepeat: ""
    property bool detailReadOnly: false
    property alias apiUrl: settingsPanel.apiUrl
    property alias apiKey: settingsPanel.apiKey
    property alias modelName: settingsPanel.modelName
    property bool inputActiveFocus: (panelRoot.panel === "ai" && aiPanel.inputActiveFocus) ||
                                    (panelRoot.panel === "detail" && detailPanel.inputActiveFocus) ||
                                    (panelRoot.panel === "settings" && settingsPanel.inputActiveFocus)

    signal closeRequested()
    signal runAiRequested(string requirement, string contextText)
    signal saveDetailRequested(string text)
    signal repeatDetailRequested(string repeat)
    signal saveSettingsRequested(string url, string key, string model)
    signal clearCompletedRequested()
    signal clearCompletedAllRequested()
    signal clearCurrentRequested()
    signal clearAllRequested()
    signal exportJsonRequested()
    signal importJsonRequested()
    signal exportMarkdownRequested()

    onPanelChanged: settingsPanel.clearAllArmed = false

    function releaseInputFocus() {
        if (panelRoot.panel === "ai")
            aiPanel.releaseInputFocus()
        else if (panelRoot.panel === "detail")
            detailPanel.releaseInputFocus()
        else if (panelRoot.panel === "settings")
            settingsPanel.releaseInputFocus()
    }

    color: panelRoot.workspaceSurfaceColor
    clip: true
    antialiasing: true
    border.width: 1
    border.color: tokens.line
    opacity: panelRoot.width > 8 ? 1 : 0

    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

    Button {
        id: closeButton
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: tokens.space3
        width: 30
        height: 30
        hoverEnabled: true
        z: 5
        onClicked: panelRoot.closeRequested()
        contentItem: Text {
            text: "×"
            color: closeButton.hovered ? tokens.danger : tokens.muted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 16
            font.weight: Font.Bold
            font.family: tokens.fontUi
            renderType: Text.NativeRendering
        }
        background: Rectangle {
            radius: tokens.radiusSm
            color: closeButton.hovered ? tokens.dangerSoft : "#00ffffff"
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    AiSummaryPanel {
        id: aiPanel
        anchors.fill: parent
        anchors.leftMargin: tokens.space4
        anchors.rightMargin: tokens.space4
        anchors.topMargin: 42
        anchors.bottomMargin: tokens.space4
        visible: panelRoot.panel === "ai"
        aiBusy: panelRoot.aiBusy
        aiResultText: panelRoot.aiResultText
        summaryScope: panelRoot.summaryScope
        summaryContextText: panelRoot.summaryContextText
        summaryEmpty: panelRoot.summaryEmpty
        surfaceColor: panelRoot.workspaceSurfaceColor
        onRunRequested: function(requirement, contextText) {
            panelRoot.runAiRequested(requirement, contextText)
        }
    }

    DetailPanel {
        id: detailPanel
        anchors.fill: parent
        anchors.leftMargin: tokens.space4
        anchors.rightMargin: tokens.space4
        anchors.topMargin: 42
        anchors.bottomMargin: tokens.space4
        visible: panelRoot.panel === "detail"
        eventText: panelRoot.detailText
        eventMeta: panelRoot.detailMeta
        eventRepeat: panelRoot.detailRepeat
        readOnly: panelRoot.detailReadOnly
        onSaveRequested: function(text) {
            panelRoot.saveDetailRequested(text)
        }
        onRepeatRequested: function(repeat) {
            panelRoot.repeatDetailRequested(repeat)
        }
    }

    SettingsPanel {
        id: settingsPanel
        anchors.fill: parent
        anchors.leftMargin: tokens.space4
        anchors.rightMargin: tokens.space4
        anchors.topMargin: 42
        anchors.bottomMargin: tokens.space4
        visible: panelRoot.panel === "settings"
        surfaceColor: panelRoot.workspaceSurfaceColor
        onSaveRequested: function(url, key, model) {
            panelRoot.saveSettingsRequested(url, key, model)
        }
        onClearCompletedRequested: panelRoot.clearCompletedRequested()
        onClearCompletedAllRequested: panelRoot.clearCompletedAllRequested()
        onClearCurrentRequested: panelRoot.clearCurrentRequested()
        onClearAllRequested: panelRoot.clearAllRequested()
        onExportJsonRequested: panelRoot.exportJsonRequested()
        onImportJsonRequested: panelRoot.importJsonRequested()
        onExportMarkdownRequested: panelRoot.exportMarkdownRequested()
    }
}
