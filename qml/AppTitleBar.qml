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
    property bool showWindowControls: true

    signal moveRequested()
    signal workspaceRequested(string workspace)
    signal aiClicked()
    signal settingsClicked()
    signal minimizeClicked()
    signal maximizeRestoreClicked()
    signal closeClicked()

    height: 40
    color: titlebar.tokens.transparent

    DragHandler {
        target: null
        onActiveChanged: if (active) titlebar.moveRequested()
    }

    Row {
        id: workspaceSwitcher
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.right: titleActions.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: titlebar.tokens.space1
        clip: true

        ToolPill {
            objectName: "stickiesWorkspaceButton"
            text: "便签"
            widthHint: 48
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            active: titlebar.workspace === "notes"
            accessibleDescription: active ? "当前便签工作区" : "切换到便签工作区"
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
            projectStyle: true
            accessibleDescription: active ? "当前项目工作区" : "切换到项目工作区"
            onClicked: titlebar.workspaceRequested("projects")
        }
    }

    Row {
        id: titleActions
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: implicitWidth
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
            accessibleDescription: active ? "智能摘要面板已打开" : "打开智能摘要面板"
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
            accessibleDescription: active ? "设置面板已打开" : "打开设置面板"
            onClicked: titlebar.settingsClicked()
        }

        WindowButton {
            objectName: "minimizeWindowButton"
            iconSource: "qrc:/assets/icons/minimize.svg"
            accessibleName: "最小化"
            accessibleDescription: "最小化 ChronoNotes 窗口"
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            visible: titlebar.showWindowControls
            onClicked: titlebar.minimizeClicked()
        }

        WindowButton {
            objectName: "maximizeWindowButton"
            iconSource: titlebar.windowVisibility === Window.Maximized
                        ? "qrc:/assets/icons/restore.svg"
                        : "qrc:/assets/icons/maximize.svg"
            accessibleName: titlebar.windowVisibility === Window.Maximized ? "还原窗口" : "最大化窗口"
            accessibleDescription: titlebar.windowVisibility === Window.Maximized
                                   ? "将 ChronoNotes 还原为窗口" : "最大化 ChronoNotes 窗口"
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            visible: titlebar.showWindowControls
            onClicked: titlebar.maximizeRestoreClicked()
        }

        WindowButton {
            objectName: "closeWindowButton"
            iconSource: "qrc:/assets/icons/close.svg"
            hoverIconSource: "qrc:/assets/icons/close-light.svg"
            accessibleName: "关闭"
            accessibleDescription: "关闭 ChronoNotes"
            danger: true
            theme: titlebar.tokens
            uiFontFamily: titlebar.uiFontFamily
            uiFontSize: titlebar.uiFontSize
            visible: titlebar.showWindowControls
            onClicked: titlebar.closeClicked()
        }
    }
}
