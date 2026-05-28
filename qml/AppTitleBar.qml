import QtQuick
import QtQuick.Window

Rectangle {
    id: titlebar

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

        Repeater {
            model: 3
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: "#c6d0df"
            }
        }

        Image {
            width: 24
            height: 24
            source: "qrc:/assets/chrono_notes_logo.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        ToolPill {
            text: "笔记"
            widthHint: 48
            active: titlebar.workspace === "notes"
            onClicked: titlebar.workspaceRequested("notes")
        }

        ToolPill {
            text: "项目树"
            widthHint: 62
            active: titlebar.workspace === "projects"
            primary: titlebar.workspace === "projects"
            onClicked: titlebar.workspaceRequested("projects")
        }

        Text {
            width: Math.max(110, titleIdentity.width - 236)
            text: titlebar.titleText
            color: "#40577a"
            font.pixelSize: 14
            font.weight: Font.Bold
            font.family: "Microsoft YaHei UI"
            elide: Text.ElideRight
            renderType: Text.NativeRendering
            anchors.verticalCenter: parent.verticalCenter
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
            text: "智能摘要"
            widthHint: 76
            primary: true
            active: titlebar.activePanel === "ai"
            onClicked: titlebar.aiClicked()
        }

        ToolPill {
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
