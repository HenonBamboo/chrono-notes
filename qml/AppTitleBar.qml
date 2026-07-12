import QtQuick
import QtQuick.Window

Rectangle {
    id: titlebar

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: titlebar.uiFontFamily
        baseFontSize: titlebar.uiFontSize
    }
    readonly property var tokens: titlebar.theme ? titlebar.theme : titlebar.fallbackTokens

    property string titleText: ""
    property string activePanel: ""
    property string workspace: "notes"
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    property int windowVisibility: Window.Windowed

    signal moveRequested()
    signal workspaceRequested(string workspace)
    signal aiClicked()
    signal settingsClicked()
    signal minimizeClicked()
    signal maximizeRestoreClicked()
    signal closeClicked()

    height: 40
    color: "transparent"

    DragHandler {
        target: null
        onActiveChanged: if (active) titlebar.moveRequested()
    }

    Row {
        id: titleIdentity
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.right: titleActions.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 9
        clip: true

        Image {
            width: 24
            height: 24
            source: "qrc:/assets/chrono_notes_logo.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        ToolPill {
            objectName: "stickiesWorkspaceButton"
            text: "便签"
            widthHint: 48
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            active: titlebar.workspace === "notes"
            onClicked: titlebar.workspaceRequested("notes")
        }

        ToolPill {
            objectName: "projectsWorkspaceButton"
            text: "项目树"
            widthHint: 62
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            active: titlebar.workspace === "projects"
            primary: titlebar.workspace === "projects"
            onClicked: titlebar.workspaceRequested("projects")
        }
    }

    Row {
        id: titleActions
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 250
        spacing: 7
        z: 2

        ToolPill {
            objectName: "aiSummaryButton"
            text: "智能摘要"
            widthHint: 76
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            active: titlebar.activePanel === "ai"
            onClicked: titlebar.aiClicked()
        }

        ToolPill {
            objectName: "settingsButton"
            text: "设置"
            widthHint: 50
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            active: titlebar.activePanel === "settings"
            onClicked: titlebar.settingsClicked()
        }

        WindowButton {
            label: "−"
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            onClicked: titlebar.minimizeClicked()
        }

        WindowButton {
            label: titlebar.windowVisibility === Window.Maximized ? "❐" : "□"
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            onClicked: titlebar.maximizeRestoreClicked()
        }

        WindowButton {
            label: "×"
            danger: true
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            onClicked: titlebar.closeClicked()
        }
    }
}
