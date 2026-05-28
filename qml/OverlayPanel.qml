import QtQuick
import QtQuick.Controls

Rectangle {
    id: panelRoot

    property string panel: ""
    property bool aiBusy: false
    property string aiResultText: ""
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
    signal runAiRequested(string requirement)
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

    color: "#f7f9ed"
    clip: true
    antialiasing: true
    border.width: 0
    opacity: panelRoot.width > 8 ? 1 : 0

    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

    Rectangle {
        anchors.fill: parent
        z: -1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#eef6ff" }
            GradientStop { position: 1.0; color: "#fff8cf" }
        }
        opacity: 0.68
    }

    Button {
        id: closeButton
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 18
        width: 34
        height: 34
        hoverEnabled: true
        z: 5
        onClicked: panelRoot.closeRequested()
        contentItem: Text {
            text: "×"
            color: "#c93636"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 16
            font.weight: Font.Bold
            font.family: "Microsoft YaHei UI"
            renderType: Text.NativeRendering
        }
        background: Rectangle {
            radius: 12
            color: closeButton.hovered ? "#ffe4e4" : "#fff1f1"
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    AiSummaryPanel {
        id: aiPanel
        anchors.fill: parent
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        anchors.topMargin: 28
        anchors.bottomMargin: 22
        visible: panelRoot.panel === "ai"
        aiBusy: panelRoot.aiBusy
        aiResultText: panelRoot.aiResultText
        onRunRequested: function(requirement) {
            panelRoot.runAiRequested(requirement)
        }
    }

    DetailPanel {
        id: detailPanel
        anchors.fill: parent
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        anchors.topMargin: 28
        anchors.bottomMargin: 22
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
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        anchors.topMargin: 28
        anchors.bottomMargin: 22
        visible: panelRoot.panel === "settings"
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
