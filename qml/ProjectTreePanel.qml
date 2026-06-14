pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: panel

    readonly property ChronoTokens tokens: ChronoTokens {}

    property var model
    property int selectedNodeId: 0
    property color inkColor: panel.tokens.ink
    property color mutedColor: panel.tokens.muted
    property color accentColor: panel.tokens.accentMint
    property string noticeText: ""
    property string selectedTitle: ""
    property string selectedKind: ""
    property string selectedPath: ""
    property string selectedDescription: ""
    property bool selectedCompleted: false
    property string selectedProgressText: ""
    property int selectedChildCount: 0
    property int selectedTotalTasks: 0
    property int selectedCompletedTasks: 0
    property int editingNodeId: 0
    property string editingOriginalTitle: ""
    property int pendingCreatedNodeId: 0
    property int activeCheckNodeId: 0
    property string aiContextText: ""
    property bool aiSummaryAvailable: false
    property string draftText: ""
    readonly property int treeColumnWidth: Math.min(280, Math.max(248, Math.round(width * 0.32)))
    readonly property int treeRowHeight: 42
    readonly property int taskCompleteControlSize: 20

    signal noticeRequested(string message)

    function indentForDepth(depth) {
        const normal = Math.min(depth, 8) * 14
        const compressed = Math.max(depth - 8, 0) * 4
        return normal + compressed
    }

    function clearTransientPressState() {
        activeCheckNodeId = 0
    }

    function markCheckPressed(nodeId) {
        activeCheckNodeId = nodeId
    }

    function clearSelection() {
        selectedNodeId = 0
        selectedTitle = ""
        selectedKind = ""
        selectedPath = ""
        selectedDescription = ""
        selectedCompleted = false
        selectedProgressText = ""
        selectedChildCount = 0
        selectedTotalTasks = 0
        selectedCompletedTasks = 0
        editingNodeId = 0
        editingOriginalTitle = ""
        pendingCreatedNodeId = 0
        clearTransientPressState()
        refreshAiContext()
    }

    function setSelection(nodeId, title, kind, childCount, totalTasks, completedTasks, completed, depth, description) {
        clearTransientPressState()
        selectedNodeId = nodeId
        selectedTitle = title
        selectedKind = kind
        selectedChildCount = childCount
        selectedTotalTasks = totalTasks
        selectedCompletedTasks = completedTasks
        selectedCompleted = completed
        selectedDescription = description || ""
        selectedPath = "根项目 / " + title
        selectedProgressText = kind === "project"
                ? "进度 " + completedTasks + "/" + totalTasks + " · 子项 " + childCount
                : (childCount > 0 ? "子项 " + childCount + " · 完成 " + completedTasks + "/" + totalTasks : (completed ? "任务已完成" : "任务未完成"))
        noticeText = ""
        if (descriptionEditor)
            descriptionEditor.text = selectedDescription
        refreshAiContext()
    }

    function selectCreated(nodeId, title, kind) {
        setSelection(nodeId, title, kind, 0, kind === "task" ? 1 : 0, 0, false, kind === "task" ? 1 : 0, "")
        beginEditing(nodeId, title)
        pendingCreatedNodeId = nodeId
    }

    function beginEditing(nodeId, title) {
        editingNodeId = nodeId
        editingOriginalTitle = title
    }

    function commitEditing(title) {
        const trimmed = title.trim()
        if (editingNodeId <= 0)
            return
        if (trimmed.length === 0) {
            if (pendingCreatedNodeId === editingNodeId) {
                model.removeNode(editingNodeId)
                clearSelection()
            } else {
                model.updateTitle(editingNodeId, editingOriginalTitle)
                if (selectedNodeId === editingNodeId)
                    selectedTitle = editingOriginalTitle
            }
            noticeText = "名称不能为空"
            noticeRequested("名称不能为空")
            editingNodeId = 0
            pendingCreatedNodeId = 0
            refreshAiContext()
            return
        }
        model.updateTitle(editingNodeId, trimmed)
        if (selectedNodeId === editingNodeId) {
            selectedTitle = trimmed
            selectedPath = "根项目 / " + trimmed
        }
        editingNodeId = 0
        pendingCreatedNodeId = 0
        noticeText = ""
        refreshAiContext()
    }

    function commitInspectorTitle(title) {
        if (selectedNodeId <= 0)
            return
        const trimmed = title.trim()
        if (trimmed.length === 0) {
            noticeText = "名称不能为空"
            noticeRequested("名称不能为空")
            return
        }
        model.updateTitle(selectedNodeId, trimmed)
        selectedTitle = trimmed
        selectedPath = "根项目 / " + trimmed
        refreshAiContext()
    }

    function commitDescription(description) {
        if (selectedNodeId <= 0)
            return
        model.updateDescription(selectedNodeId, description)
        selectedDescription = description
        refreshAiContext()
    }

    function cancelEditing() {
        if (editingNodeId <= 0)
            return
        if (pendingCreatedNodeId === editingNodeId)
            model.removeNode(editingNodeId)
        editingNodeId = 0
        pendingCreatedNodeId = 0
        refreshAiContext()
    }

    function createRootProject() {
        clearTransientPressState()
        const id = model.addProject("未命名项目")
        if (id > 0)
            selectCreated(id, "未命名项目", "project")
        return id
    }

    function createChildForSelected() {
        clearTransientPressState()
        if (selectedNodeId <= 0) {
            noticeText = "先选择一个项目或任务"
            noticeRequested("先选择一个项目或任务，再添加子项。")
            return 0
        }
        const id = model.addChild(selectedNodeId, "新子项")
        if (id > 0)
            selectCreated(id, "新子项", "task")
        return id
    }

    function removeSelectedNode() {
        if (selectedNodeId <= 0)
            return
        model.removeNode(selectedNodeId)
        clearSelection()
        noticeText = "已删除"
    }

    function removeNodeFromTree(nodeId) {
        if (nodeId <= 0)
            return
        model.removeNode(nodeId)
        if (selectedNodeId === nodeId)
            clearSelection()
    }

    function toggleSelectedComplete() {
        if (selectedNodeId <= 0 || selectedKind !== "task" || selectedChildCount > 0)
            return
        model.toggleComplete(selectedNodeId)
        selectedCompleted = !selectedCompleted
        selectedCompletedTasks = selectedCompleted ? Math.max(1, selectedCompletedTasks) : 0
        selectedProgressText = selectedCompleted ? "任务已完成" : "任务未完成"
        clearTransientPressState()
        refreshAiContext()
    }

    function addProjectFromInput() {
        return createRootProject()
    }

    function addChildFromInput() {
        return createChildForSelected()
    }

    function refreshAiContext() {
        if (!model || model.projectCount <= 0) {
            aiSummaryAvailable = false
            aiContextText = ""
            return
        }
        aiSummaryAvailable = true
        if (selectedNodeId <= 0) {
            aiContextText = "项目摘要上下文\n范围：整个项目树\n项目数：" + model.projectCount
                    + "\n任务：" + model.completedTasks + "/" + model.totalTasks
            return
        }
        aiContextText = "项目摘要上下文\n标题：" + selectedTitle
                + "\n类型：" + (selectedKind === "project" ? "项目" : "任务")
                + "\n路径：" + selectedPath
                + "\n进度：" + selectedCompletedTasks + "/" + selectedTotalTasks
                + "\n直接子项：" + selectedChildCount
                + "\n具体内容：" + selectedDescription
                + "\n状态：" + (selectedKind === "task" ? (selectedCompleted ? "已完成" : "未完成") : "项目推进中")
    }

    Rectangle {
        anchors.fill: parent
        radius: panel.tokens.radiusLg
        color: "#10ffffff"
        border.width: 1
        border.color: panel.tokens.lineSoft
        antialiasing: true
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: panel.clearTransientPressState()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: panel.tokens.space2
        spacing: panel.tokens.space3

        Rectangle {
            id: treePane
            objectName: "projectTreePane"
            Layout.preferredWidth: panel.treeColumnWidth
            Layout.fillHeight: true
            radius: panel.tokens.radiusMd
            color: "#34fffef7"
            border.width: 1
            border.color: "#1688c57f"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: panel.tokens.space2
                spacing: panel.tokens.space2

                RowLayout {
                    id: toolbar
                    objectName: "projectTreeToolbar"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    spacing: panel.tokens.space2

                    ToolPill {
                        objectName: "projectCreateRootButton"
                        text: "+ 项目"
                        widthHint: 68
                        primary: true
                        onClicked: panel.createRootProject()
                    }

                    Text {
                        objectName: "projectCountText"
                        Layout.fillWidth: true
                        text: "项目 " + panel.model.projectCount + " · 任务 " + panel.model.completedTasks + "/" + panel.model.totalTasks
                        color: panel.mutedColor
                        font.pixelSize: 12
                        font.family: panel.tokens.fontUi
                        elide: Text.ElideRight
                        renderType: Text.NativeRendering
                    }
                }

                ListView {
                    id: treeView
                    objectName: "projectTreeViewport"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: panel.model
                    spacing: panel.tokens.space1
                    boundsBehavior: Flickable.DragAndOvershootBounds

                    delegate: Rectangle {
                        id: row

                        required property int nodeId
                        required property int parentId
                        required property string title
                        required property string kind
                        required property string description
                        required property bool completed
                        required property int depth
                        required property bool expanded
                        required property int childCount
                        required property int totalTasks
                        required property int completedTasks

                        width: treeView.width
                        height: panel.treeRowHeight
                        radius: panel.tokens.radiusSm
                        color: panel.selectedNodeId === nodeId ? "#66fffef7" : "#00ffffff"
                        border.width: panel.selectedNodeId === nodeId ? 1 : 0
                        border.color: "#6688c57f"
                        antialiasing: true

                        Rectangle {
                            width: panel.selectedNodeId === row.nodeId ? 3 : 0
                            height: parent.height - 12
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 2
                            color: panel.accentColor
                        }

                        Repeater {
                            model: Math.min(row.depth, 8)
                            Rectangle {
                                required property int index

                                width: 1
                                height: row.height - 10
                                x: panel.tokens.space2 + index * 14 + 9
                                y: 5
                                color: "#2688c57f"
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: panel.setSelection(row.nodeId, row.title, row.kind, row.childCount, row.totalTasks, row.completedTasks, row.completed, row.depth, row.description)
                            onDoubleClicked: panel.beginEditing(row.nodeId, row.title)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: panel.tokens.space2 + panel.indentForDepth(row.depth)
                            anchors.rightMargin: panel.tokens.space1
                            spacing: panel.tokens.space1

                            Item {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                visible: row.childCount > 0

                                Text {
                                    anchors.centerIn: parent
                                    text: row.expanded ? "⌄" : "›"
                                    color: panel.mutedColor
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    font.family: panel.tokens.fontUi
                                    renderType: Text.NativeRendering
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        panel.clearTransientPressState()
                                        panel.model.toggleExpanded(row.nodeId)
                                    }
                                }
                            }

                            Item {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                visible: row.childCount === 0
                            }

                            Item {
                                Layout.preferredWidth: panel.taskCompleteControlSize
                                Layout.preferredHeight: panel.taskCompleteControlSize
                                visible: row.kind === "task"

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 5
                                    color: row.completed ? panel.accentColor : "#00ffffff"
                                    border.width: 1
                                    border.color: panel.activeCheckNodeId === row.nodeId ? panel.tokens.accentBlue
                                                  : row.completed ? panel.accentColor : "#7790a3a7"
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: row.completed ? "✓" : ""
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    font.family: panel.tokens.fontUi
                                    renderType: Text.NativeRendering
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: row.childCount === 0
                                    onPressed: panel.markCheckPressed(row.nodeId)
                                    onClicked: {
                                        panel.model.toggleComplete(row.nodeId)
                                        panel.setSelection(row.nodeId, row.title, row.kind, row.childCount, row.totalTasks, row.completed ? row.completedTasks - 1 : row.completedTasks + 1, !row.completed, row.depth, row.description)
                                        panel.clearTransientPressState()
                                    }
                                    onCanceled: panel.clearTransientPressState()
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                visible: row.kind !== "task"
                                radius: panel.tokens.radiusXs
                                color: "#2288c57f"
                                border.width: 1
                                border.color: "#4488c57f"

                                Text {
                                    anchors.centerIn: parent
                                    text: "项"
                                    color: "#376c3a"
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    font.family: panel.tokens.fontUi
                                    renderType: Text.NativeRendering
                                }
                            }

                            TextField {
                                id: editField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                visible: panel.editingNodeId === row.nodeId
                                text: row.title
                                selectByMouse: true
                                font.pixelSize: 12
                                font.family: panel.tokens.fontUi
                                color: panel.inkColor
                                background: Rectangle {
                                    radius: panel.tokens.radiusSm
                                    color: panel.tokens.card
                                    border.width: 1
                                    border.color: panel.accentColor
                                }
                                onAccepted: panel.commitEditing(text)
                                Keys.onEscapePressed: panel.cancelEditing()
                                onVisibleChanged: if (visible) Qt.callLater(forceActiveFocus)
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                visible: panel.editingNodeId !== row.nodeId
                                spacing: 0

                                Text {
                                    Layout.fillWidth: true
                                    text: row.title
                                    color: row.completed ? "#6d7d81" : panel.inkColor
                                    font.pixelSize: 12
                                    font.weight: row.kind === "project" ? Font.Bold : Font.DemiBold
                                    font.family: panel.tokens.fontUi
                                    elide: Text.ElideRight
                                    renderType: Text.NativeRendering
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: row.kind === "project"
                                          ? row.completedTasks + "/" + row.totalTasks
                                          : (row.childCount > 0 ? row.completedTasks + "/" + row.totalTasks + " · 子项 " + row.childCount : (row.completed ? "完成" : "任务"))
                                    color: panel.mutedColor
                                    font.pixelSize: 10
                                    font.family: panel.tokens.fontUi
                                    elide: Text.ElideRight
                                    renderType: Text.NativeRendering
                                }
                            }

                            ToolPill {
                                Layout.preferredWidth: 28
                                text: "+"
                                widthHint: 28
                                visible: panel.selectedNodeId === row.nodeId && panel.editingNodeId !== row.nodeId
                                onClicked: {
                                    panel.setSelection(row.nodeId, row.title, row.kind, row.childCount, row.totalTasks, row.completedTasks, row.completed, row.depth, row.description)
                                    panel.createChildForSelected()
                                }
                            }

                            ToolPill {
                                objectName: "projectTreeDeleteButton"
                                Layout.preferredWidth: 34
                                text: "-"
                                widthHint: 34
                                danger: true
                                visible: panel.selectedNodeId === row.nodeId && panel.editingNodeId !== row.nodeId
                                onClicked: panel.removeNodeFromTree(row.nodeId)
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 32, 260)
                        visible: treeView.count === 0
                        text: "还没有项目\n点击“+ 项目”后直接编辑名称"
                        color: panel.tokens.mutedSoft
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        lineHeight: 1.35
                        font.family: panel.tokens.fontUi
                        renderType: Text.NativeRendering
                    }
                }
            }
        }

        Rectangle {
            id: inspectorPane
            objectName: "projectInspectorPane"
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: panel.tokens.radiusMd
            color: "#55fffef7"
            border.width: 1
            border.color: "#1688c57f"

            MouseArea {
                anchors.fill: parent
                onClicked: panel.clearTransientPressState()
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: panel.tokens.space4
                spacing: panel.tokens.space3

                Rectangle {
                    objectName: "projectSummaryBar"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: panel.tokens.radiusMd
                    color: panel.tokens.card
                    border.width: 1
                    border.color: panel.tokens.lineSoft

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: panel.tokens.space3
                        anchors.rightMargin: panel.tokens.space3
                        spacing: panel.tokens.space3

                        Text {
                            objectName: "projectTaskCountText"
                            text: "任务 " + panel.model.completedTasks + "/" + panel.model.totalTasks
                            color: panel.inkColor
                            font.pixelSize: panel.tokens.sizeBody
                            font.weight: Font.Bold
                            font.family: panel.tokens.fontUi
                            renderType: Text.NativeRendering
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 7
                            radius: 4
                            color: "#dbe7e4"
                            clip: true

                            Rectangle {
                                width: parent.width * (panel.model.totalTasks > 0 ? panel.model.completedTasks / panel.model.totalTasks : 0)
                                height: parent.height
                                radius: 4
                                color: panel.accentColor
                            }
                        }
                    }
                }

                Rectangle {
                    objectName: "projectInspectorCard"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: panel.tokens.radiusMd
                    color: panel.tokens.card
                    border.width: 1
                    border.color: panel.tokens.lineSoft

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: panel.tokens.space4
                        spacing: panel.tokens.space3

                        TextField {
                            id: titleEditor
                            objectName: "projectInspectorTitleEditor"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            enabled: panel.selectedNodeId > 0
                            text: panel.selectedTitle
                            placeholderText: "选择左侧项目或任务"
                            selectByMouse: true
                            font.pixelSize: 18
                            font.weight: Font.Bold
                            font.family: panel.tokens.fontUi
                            color: panel.inkColor
                            renderType: Text.NativeRendering
                            onAccepted: panel.commitInspectorTitle(text)
                            onEditingFinished: panel.commitInspectorTitle(text)
                            background: Rectangle {
                                radius: panel.tokens.radiusMd
                                color: titleEditor.enabled ? "#fbfff7" : "#55fffef7"
                                border.width: titleEditor.activeFocus ? 1 : 0
                                border.color: panel.accentColor
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: panel.selectedNodeId > 0
                                  ? (panel.selectedKind === "project" ? "项目 · " : "任务 · ") + panel.selectedProgressText
                                  : "左侧负责结构；右侧负责具体内容、完成状态和详情。"
                            color: panel.mutedColor
                            font.pixelSize: 13
                            font.family: panel.tokens.fontUi
                            elide: Text.ElideRight
                            renderType: Text.NativeRendering
                        }

                        ScrollView {
                            id: descriptionScroll
                            objectName: "projectDescriptionScroll"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 132
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            background: Rectangle {
                                radius: panel.tokens.radiusMd
                                color: descriptionEditor.enabled ? "#f8fff1" : "#55fffef7"
                                border.width: descriptionEditor.activeFocus ? 1 : 0
                                border.color: panel.accentColor
                            }

                            TextArea {
                                id: descriptionEditor
                                objectName: "projectDescriptionEditor"
                                width: descriptionScroll.availableWidth
                                enabled: panel.selectedNodeId > 0
                                text: panel.selectedDescription
                                placeholderText: "记录这个项目/任务的背景、要求、下一步处理"
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                font.pixelSize: 13
                                font.family: panel.tokens.fontUi
                                color: panel.inkColor
                                renderType: Text.NativeRendering
                                onActiveFocusChanged: if (!activeFocus) panel.commitDescription(text)
                                background: Rectangle {
                                    color: "transparent"
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: panel.tokens.space4
                            rowSpacing: panel.tokens.space2

                            Text { text: "路径"; color: panel.mutedColor; font.pixelSize: 12; font.family: panel.tokens.fontUi }
                            Text {
                                objectName: "projectInspectorPath"
                                Layout.fillWidth: true
                                text: panel.selectedNodeId > 0 ? panel.selectedPath : "未选择"
                                color: panel.inkColor
                                font.pixelSize: 13
                                font.family: panel.tokens.fontUi
                                elide: Text.ElideRight
                            }
                            Text { text: "类型"; color: panel.mutedColor; font.pixelSize: 12; font.family: panel.tokens.fontUi }
                            Text {
                                text: panel.selectedKind === "project" ? "项目" : (panel.selectedKind === "task" ? "任务" : "未选择")
                                color: panel.inkColor
                                font.pixelSize: 13
                                font.family: panel.tokens.fontUi
                            }
                            Text { text: "进度"; color: panel.mutedColor; font.pixelSize: 12; font.family: panel.tokens.fontUi }
                            Text {
                                objectName: "projectInspectorProgress"
                                text: panel.selectedNodeId > 0 ? panel.selectedProgressText : "未选择节点"
                                color: panel.inkColor
                                font.pixelSize: 13
                                font.family: panel.tokens.fontUi
                            }
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: panel.tokens.space2

                            ToolPill {
                                objectName: "projectInspectorCompleteButton"
                                text: panel.selectedKind === "task" && panel.selectedChildCount > 0
                                      ? "子项汇总"
                                      : (panel.selectedCompleted ? "标为未完成" : "标为完成")
                                widthHint: panel.selectedKind === "task" && panel.selectedChildCount > 0 ? 82 : 90
                                enabled: panel.selectedKind === "task" && panel.selectedChildCount === 0
                                onClicked: panel.toggleSelectedComplete()
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: panel.model
        function onTreeChanged() {
            panel.refreshAiContext()
        }
    }

    Component.onCompleted: refreshAiContext()
}
