import QtQuick
import QtQuick.Window

Rectangle {
    id: titlebar

    readonly property ChronoTokens tokens: ChronoTokens {}

    property string titleText: ""
    property string activePanel: ""
    property string workspace: "notes"
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
            active: titlebar.workspace === "notes"
            onClicked: titlebar.workspaceRequested("notes")
        }

        ToolPill {
            objectName: "projectsWorkspaceButton"
            text: "项目树"
            widthHint: 62
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
            primary: titlebar.activePanel === "ai"
            active: titlebar.activePanel === "ai"
            onClicked: titlebar.aiClicked()
        }

        ToolPill {
            objectName: "settingsButton"
            text: "设置"
            widthHint: 50
            active: titlebar.activePanel === "settings"
            onClicked: titlebar.settingsClicked()
        }

        WindowButton {
            label: "−"
            onClicked: titlebar.minimizeClicked()
        }

        WindowButton {
            label: titlebar.windowVisibility === Window.Maximized ? "❐" : "□"
            onClicked: titlebar.maximizeRestoreClicked()
        }

        WindowButton {
            label: "×"
            danger: true
            onClicked: titlebar.closeClicked()
        }
    }
}
