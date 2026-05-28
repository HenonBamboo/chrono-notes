pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: panel

    property var model
    property int selectedNodeId: 0
    property color inkColor: "#0f1f24"
    property color mutedColor: "#5d7277"
    property color accentColor: "#2f8f83"

    signal noticeRequested(string message)

    function addProjectFromInput() {
        const title = input.text.trim()
        if (title.length === 0) {
            panel.noticeRequested("请输入项目名称。")
            return
        }
        const id = panel.model.addProject(title)
        if (id > 0) {
            panel.selectedNodeId = id
            input.text = ""
        }
    }

    function addChildFromInput() {
        const title = input.text.trim()
        if (panel.selectedNodeId <= 0) {
            panel.noticeRequested("先选中一个项目或任务，再分化子任务。")
            return
        }
        if (title.length === 0) {
            panel.noticeRequested("请输入子任务名称。")
            return
        }
        const id = panel.model.addChild(panel.selectedNodeId, title)
        if (id > 0) {
            panel.selectedNodeId = id
            input.text = ""
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 26
        color: "#12eef9f5"
        border.width: 0
        antialiasing: true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.bottomMargin: 8
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            spacing: 10

            TextField {
                id: input
                Layout.fillWidth: true
                Layout.fillHeight: true
                placeholderText: panel.selectedNodeId > 0 ? "输入子任务，回车分化到选中项" : "输入项目名称，回车创建项目"
                font.pixelSize: 14
                font.family: "Microsoft YaHei UI"
                color: panel.inkColor
                selectionColor: "#9fd7ce"
                selectedTextColor: panel.inkColor
                renderType: Text.NativeRendering
                background: Rectangle {
                    radius: 15
                    color: "#ddffffff"
                    border.color: input.activeFocus ? "#662f8f83" : "#22647766"
                    border.width: 1
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                }
                onAccepted: panel.selectedNodeId > 0 ? panel.addChildFromInput() : panel.addProjectFromInput()
            }

            ToolPill {
                text: "建项目"
                widthHint: 72
                primary: true
                onClicked: panel.addProjectFromInput()
            }

            ToolPill {
                text: "分化"
                widthHint: 60
                onClicked: panel.addChildFromInput()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            spacing: 14

            Text {
                text: "项目 " + panel.model.projectCount
                color: panel.mutedColor
                font.pixelSize: 12
                font.family: "Microsoft YaHei UI"
                renderType: Text.NativeRendering
            }

            Text {
                text: "任务 " + panel.model.completedTasks + "/" + panel.model.totalTasks
                color: panel.mutedColor
                font.pixelSize: 12
                font.family: "Microsoft YaHei UI"
                renderType: Text.NativeRendering
            }

            Rectangle {
                Layout.preferredWidth: 220
                Layout.preferredHeight: 7
                radius: 4
                color: "#dbe7e4"
                clip: true

                Rectangle {
                    width: parent.width * (panel.model.totalTasks > 0 ? panel.model.completedTasks / panel.model.totalTasks : 0)
                    height: parent.height
                    radius: 4
                    color: panel.accentColor
                    Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                }
            }

            Item { Layout.fillWidth: true }
        }

        ListView {
            id: treeView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: panel.model
            spacing: 8
            boundsBehavior: Flickable.DragAndOvershootBounds

            delegate: Rectangle {
                id: row

                required property int nodeId
                required property int parentId
                required property string title
                required property string kind
                required property bool completed
                required property int depth
                required property bool expanded
                required property int childCount
                required property int totalTasks
                required property int completedTasks

                width: treeView.width
                height: 58
                radius: 18
                color: panel.selectedNodeId === nodeId ? "#d8f0eb" : "#f9fffc"
                border.width: panel.selectedNodeId === nodeId ? 1 : 0
                border.color: "#7bbab0"
                antialiasing: true

                MouseArea {
                    anchors.fill: parent
                    onClicked: panel.selectedNodeId = row.nodeId
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12 + Math.min(row.depth, 6) * 22
                    anchors.rightMargin: 12
                    spacing: 8

                    Button {
                        id: expandButton
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        visible: row.childCount > 0
                        hoverEnabled: true
                        text: row.expanded ? "收" : "展"
                        onClicked: panel.model.toggleExpanded(row.nodeId)
                        contentItem: Text {
                            text: expandButton.text
                            color: "#42656a"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.family: "Microsoft YaHei UI"
                            fontSizeMode: Text.HorizontalFit
                            minimumPixelSize: 9
                            renderType: Text.NativeRendering
                        }
                        background: Rectangle {
                            radius: 10
                            color: expandButton.hovered ? "#e5f4f0" : "#edf7f5"
                        }
                    }

                    Item {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        visible: row.childCount === 0
                    }

                    CheckBox {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        visible: row.kind === "task"
                        checked: row.completed
                        onClicked: panel.model.toggleComplete(row.nodeId)
                    }

                    Rectangle {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        visible: row.kind !== "task"
                        radius: 10
                        color: "#2f8f83"

                        Text {
                            anchors.centerIn: parent
                            text: "项"
                            color: "white"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            font.family: "Microsoft YaHei UI"
                            renderType: Text.NativeRendering
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 3

                        Text {
                            Layout.fillWidth: true
                            text: row.title
                            color: row.completed ? "#6d7d81" : panel.inkColor
                            font.pixelSize: row.kind === "project" ? 15 : 14
                            font.weight: row.kind === "project" ? Font.Bold : Font.DemiBold
                            font.family: "Microsoft YaHei UI"
                            elide: Text.ElideRight
                            renderType: Text.NativeRendering
                        }

                        Text {
                            Layout.fillWidth: true
                            text: row.kind === "project"
                                  ? "进度 " + row.completedTasks + "/" + row.totalTasks + " · 子项 " + row.childCount
                                  : (row.childCount > 0 ? "子项 " + row.childCount + " · 完成 " + row.completedTasks + "/" + row.totalTasks : "任务")
                            color: panel.mutedColor
                            font.pixelSize: 11
                            font.family: "Microsoft YaHei UI"
                            elide: Text.ElideRight
                            renderType: Text.NativeRendering
                        }
                    }

                    ToolPill {
                        text: "分化"
                        widthHint: 54
                        onClicked: {
                            panel.selectedNodeId = row.nodeId
                            panel.addChildFromInput()
                        }
                    }

                    ToolPill {
                        text: "删"
                        widthHint: 42
                        danger: true
                        onClicked: panel.model.removeNode(row.nodeId)
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: Math.min(parent.width - 80, 460)
                visible: treeView.count === 0
                text: "还没有项目。\n先建一个项目，再把它分化成能完成的任务。"
                color: "#789094"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                lineHeight: 1.4
                font.family: "Microsoft YaHei UI"
                renderType: Text.NativeRendering
            }
        }
    }
}
