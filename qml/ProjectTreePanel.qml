pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: panel
    objectName: "projectTreePanel"

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: panel.uiFontFamily
        baseFontSize: panel.uiFontSize
    }
    readonly property var tokens: panel.theme ? panel.theme : panel.fallbackTokens

    property var model: null
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 14
    property color inkColor: tokens.ink
    property color mutedColor: tokens.textSecondary
    property color accentColor: tokens.projectAccent
    property string noticeText: ""
    property int activeCheckNodeId: -1
    property int pendingCreatedNodeId: -1
    readonly property int editingNodeId: 0

    readonly property int selectedNodeId: model ? model.selectedNodeId : -1
    readonly property string selectedTitle: model ? model.selectedNodeTitle : ""
    readonly property string selectedKind: model ? model.selectedNodeKind : ""
    readonly property string selectedDescription: model ? model.selectedNodeDescription : ""
    readonly property var selectedAncestorPath: model ? model.selectedAncestorPath : []
    readonly property int selectedChildCount: model && model.selectedNodeChildCount !== undefined
                                              ? model.selectedNodeChildCount : -1
    readonly property bool selectedCompleted: model && model.selectedNodeCompleted !== undefined
                                               ? model.selectedNodeCompleted : false
    readonly property string selectedPath: selectedNodeId > 0
                                           ? selectedAncestorPath.concat([selectedTitle]).join(" / ")
                                           : ""
    readonly property bool canToggleSelectedComplete: selectedNodeId > 0 &&
                                                       selectedKind === "task" &&
                                                       selectedChildCount === 0
    readonly property string selectedProgressText: selectedKind === "project"
                                                   ? "全树任务 " + safeCompletedTasks() + "/" + safeTotalTasks()
                                                   : selectedKind === "task"
                                                     ? "由任务自身状态控制"
                                                     : "未选择节点"
    readonly property bool aiSummaryAvailable: model ? model.projectCount > 0 : false
    readonly property string aiContextText: {
        if (!aiSummaryAvailable)
            return ""
        if (selectedNodeId <= 0) {
            return "项目摘要上下文\n范围：整个项目树"
                    + "\n项目数：" + model.projectCount
                    + "\n任务进度：" + safeCompletedTasks() + "/" + safeTotalTasks()
        }
        return "项目摘要上下文"
                + "\n标题：" + selectedTitle
                + "\n类型：" + (selectedKind === "project" ? "项目" : "任务")
                + "\n路径：" + selectedPath
                + "\n具体内容：" + selectedDescription
                + "\n全树任务进度：" + safeCompletedTasks() + "/" + safeTotalTasks()
    }

    signal noticeRequested(string message)

    function safeTotalTasks() {
        return model && model.totalTasks !== undefined ? model.totalTasks : 0
    }

    function safeCompletedTasks() {
        return model && model.completedTasks !== undefined ? model.completedTasks : 0
    }

    function syncEditors() {
        if (titleEditor.text !== selectedTitle)
            titleEditor.text = selectedTitle
        if (descriptionEditor.text !== selectedDescription)
            descriptionEditor.text = selectedDescription
    }

    function clearTransientPressState() {
        activeCheckNodeId = -1
    }

    function markCheckPressed(nodeId) {
        activeCheckNodeId = nodeId
    }

    function clearSelection() {
        clearTransientPressState()
        pendingCreatedNodeId = -1
        if (model && typeof model.selectNode === "function")
            model.selectNode(-1)
        Qt.callLater(syncEditors)
    }

    function selectNode(nodeId) {
        clearTransientPressState()
        if (!model || typeof model.selectNode !== "function" || !model.selectNode(nodeId)) {
            noticeRequested(model && model.lastError ? model.lastError : "无法选择该项目节点")
            return false
        }
        Qt.callLater(syncEditors)
        return true
    }

    function createRootProject() {
        if (!model)
            return -1
        const id = model.addProject("新项目")
        if (id <= 0) {
            noticeRequested(model.lastError || "无法创建项目")
            return -1
        }
        pendingCreatedNodeId = id
        selectNode(id)
        Qt.callLater(function() {
            syncEditors()
            titleEditor.selectAll()
            titleEditor.forceActiveFocus(Qt.TabFocusReason)
        })
        return id
    }

    function createChildForSelected() {
        if (!model || selectedNodeId <= 0) {
            noticeRequested("请先选择一个项目或任务")
            return -1
        }
        const id = model.addChild(selectedNodeId, "新子项")
        if (id <= 0) {
            noticeRequested(model.lastError || "无法创建子项")
            return -1
        }
        pendingCreatedNodeId = id
        selectNode(id)
        Qt.callLater(function() {
            syncEditors()
            titleEditor.selectAll()
            titleEditor.forceActiveFocus(Qt.TabFocusReason)
        })
        return id
    }

    function commitInspectorTitle(value) {
        if (!model || selectedNodeId <= 0)
            return false
        const trimmed = value.trim()
        if (trimmed.length === 0) {
            if (pendingCreatedNodeId === selectedNodeId) {
                model.removeNode(selectedNodeId)
                pendingCreatedNodeId = -1
                noticeRequested("已取消创建空白节点")
            } else {
                noticeRequested("标题不能为空")
                syncEditors()
            }
            return false
        }
        if (trimmed === selectedTitle) {
            pendingCreatedNodeId = -1
            return true
        }
        if (!model.updateTitle(selectedNodeId, trimmed)) {
            noticeRequested(model.lastError || "标题保存失败")
            syncEditors()
            return false
        }
        pendingCreatedNodeId = -1
        Qt.callLater(syncEditors)
        return true
    }

    function commitDescription(value) {
        if (!model || selectedNodeId <= 0)
            return false
        if (value === selectedDescription)
            return true
        if (!model.updateDescription(selectedNodeId, value)) {
            noticeRequested(model.lastError || "内容保存失败")
            syncEditors()
            return false
        }
        Qt.callLater(syncEditors)
        return true
    }

    function removeSelected() {
        if (!model || selectedNodeId <= 0)
            return false
        if (!model.removeNode(selectedNodeId)) {
            noticeRequested(model.lastError || "删除失败")
            return false
        }
        pendingCreatedNodeId = -1
        Qt.callLater(syncEditors)
        return true
    }

    function toggleSelectedComplete() {
        if (!model || !canToggleSelectedComplete)
            return false
        if (!model.toggleComplete(selectedNodeId)) {
            noticeRequested(model.lastError || "只有不含子项的任务可以标记完成")
            return false
        }
        clearTransientPressState()
        return true
    }

    function refreshAiContext() {
        // Compatibility no-op: aiContextText is now derived from the model.
    }

    component TreeIconButton: Button {
        id: iconButton
        required property url iconSource
        required property string accessibleName
        property string accessibleDescription: accessibleName
        property bool danger: false

        implicitWidth: panel.tokens.controlHeight
        implicitHeight: panel.tokens.controlHeight
        hoverEnabled: true
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: accessibleName
        Accessible.description: accessibleDescription

        contentItem: Image {
            source: iconButton.iconSource
            sourceSize.width: 40
            sourceSize.height: 40
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        background: Rectangle {
            radius: panel.tokens.radiusSm
            color: iconButton.pressed ? panel.tokens.surfacePressed
                 : iconButton.hovered
                   ? (iconButton.danger ? panel.tokens.dangerSoft : panel.tokens.surfaceHover)
                   : panel.tokens.transparent
            border.width: iconButton.visualFocus ? 2 : 1
            border.color: iconButton.visualFocus ? panel.tokens.focusRing
                          : iconButton.danger ? panel.tokens.danger
                                              : panel.tokens.borderSubtle
            Behavior on color {
                ColorAnimation { duration: panel.tokens.motionFast; easing.type: Easing.OutCubic }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: panel.tokens.space3

        RowLayout {
            Layout.fillWidth: true
            spacing: panel.tokens.space2

            ColumnLayout {
                Layout.fillWidth: true
                spacing: panel.tokens.space1

                Text {
                    text: "项目"
                    color: panel.inkColor
                    font.family: panel.tokens.fontUi
                    font.pixelSize: panel.tokens.sizeDisplay
                    font.weight: Font.Bold
                    renderType: Text.NativeRendering
                    Accessible.role: Accessible.Heading
                    Accessible.name: "项目工作区"
                }

                Text {
                    text: panel.model
                          ? "项目 " + panel.model.projectCount + " · 任务 "
                            + panel.safeCompletedTasks() + "/" + panel.safeTotalTasks()
                          : "项目数据暂不可用"
                    color: panel.mutedColor
                    font.family: panel.tokens.fontUi
                    font.pixelSize: panel.tokens.sizeBody
                    renderType: Text.NativeRendering
                }
            }

            ToolPill {
                id: createProjectButton
                objectName: "projectCreateRootButton"
                text: "新建项目"
                widthHint: 96
                primary: true
                projectStyle: true
                theme: panel.tokens
                accessibleDescription: "在项目树根节点创建项目"
                onClicked: panel.createRootProject()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: panel.tokens.space3

            Rectangle {
                Layout.preferredWidth: Math.max(250, parent.width * 0.42)
                Layout.fillHeight: true
                radius: panel.tokens.radiusMd
                color: panel.tokens.surface
                border.width: 1
                border.color: panel.tokens.border

                ListView {
                    id: treeList
                    objectName: "projectTreeList"
                    anchors.fill: parent
                    anchors.margins: panel.tokens.space2
                    model: panel.model
                    clip: true
                    spacing: panel.tokens.space1
                    activeFocusOnTab: true
                    keyNavigationEnabled: true
                    highlightMoveDuration: panel.tokens.motionFast
                    ScrollBar.vertical: ScrollBar {}
                    Accessible.role: Accessible.Tree
                    Accessible.name: "项目树"
                    Accessible.description: "使用方向键浏览项目和任务，按回车选择"

                    delegate: Rectangle {
                        id: treeRow
                        objectName: "projectTreeRow"
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

                        width: treeList.width
                        height: panel.tokens.controlHeight + panel.tokens.space2
                        radius: panel.tokens.radiusSm
                        color: panel.selectedNodeId === nodeId
                               ? panel.tokens.selectionSurface
                               : rowMouse.containsMouse
                                 ? panel.tokens.surfaceHover
                                 : panel.tokens.transparent
                        border.width: panel.selectedNodeId === nodeId ? 1 : 0
                        border.color: panel.tokens.projectAccent
                        Accessible.role: Accessible.TreeItem
                        Accessible.name: title
                        Accessible.description: (kind === "project" ? "项目" : "任务")
                                                + (completed ? "，已完成" : "，未完成")
                        Accessible.selected: panel.selectedNodeId === nodeId

                        Keys.onReturnPressed: panel.selectNode(nodeId)
                        Keys.onEnterPressed: panel.selectNode(nodeId)

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            onClicked: panel.selectNode(treeRow.nodeId)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: panel.tokens.space2 + treeRow.depth * panel.tokens.space4
                            anchors.rightMargin: panel.tokens.space1
                            spacing: panel.tokens.space1

                            Button {
                                id: expandButton
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 32
                                visible: treeRow.childCount > 0
                                enabled: visible
                                hoverEnabled: true
                                activeFocusOnTab: visible
                                onClicked: panel.model.toggleExpanded(treeRow.nodeId)
                                Accessible.role: Accessible.Button
                                Accessible.name: treeRow.expanded ? "折叠" + treeRow.title : "展开" + treeRow.title
                                Accessible.description: "切换子项可见状态"

                                contentItem: Image {
                                    source: treeRow.expanded
                                            ? "qrc:/assets/icons/chevron-down.svg"
                                            : "qrc:/assets/icons/chevron-right.svg"
                                    sourceSize.width: 32
                                    sourceSize.height: 32
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                }
                                background: Rectangle {
                                    radius: panel.tokens.radiusXs
                                    color: expandButton.hovered ? panel.tokens.projectAccentSoft
                                                                : panel.tokens.transparent
                                    border.width: expandButton.visualFocus ? 2 : 0
                                    border.color: panel.tokens.focusRing
                                }
                            }

                            Item {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 32
                                visible: treeRow.childCount === 0
                            }

                            Image {
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                                source: treeRow.kind === "project"
                                        ? "qrc:/assets/icons/folder.svg"
                                        : "qrc:/assets/icons/task.svg"
                                sourceSize.width: 36
                                sourceSize.height: 36
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                Accessible.ignored: true
                            }

                            Button {
                                id: completionButton
                                objectName: "projectTaskCheckButton"
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                visible: treeRow.kind === "task" && treeRow.childCount === 0
                                activeFocusOnTab: visible
                                hoverEnabled: true
                                onPressed: panel.markCheckPressed(treeRow.nodeId)
                                onReleased: panel.clearTransientPressState()
                                onCanceled: panel.clearTransientPressState()
                                onClicked: {
                                    panel.selectNode(treeRow.nodeId)
                                    panel.toggleSelectedComplete()
                                }
                                Accessible.role: Accessible.CheckBox
                                Accessible.name: treeRow.completed
                                                 ? "将“" + treeRow.title + "”标为未完成"
                                                 : "将“" + treeRow.title + "”标为完成"
                                Accessible.checked: treeRow.completed

                                contentItem: Item {
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 20
                                        height: 20
                                        radius: panel.tokens.radiusXs
                                        color: treeRow.completed ? panel.tokens.projectAccent
                                                                 : panel.tokens.surface
                                        border.width: completionButton.visualFocus ? 2 : 1
                                        border.color: completionButton.visualFocus
                                                      ? panel.tokens.focusRing
                                                      : panel.tokens.projectAccent

                                        Image {
                                            anchors.centerIn: parent
                                            width: 16
                                            height: 16
                                            visible: treeRow.completed
                                            source: "qrc:/assets/icons/check.svg"
                                            sourceSize.width: 32
                                            sourceSize.height: 32
                                        }
                                    }
                                }
                                background: Item {}
                            }

                            Text {
                                Layout.fillWidth: true
                                text: treeRow.title
                                color: treeRow.completed ? panel.tokens.textDisabled : panel.inkColor
                                font.family: panel.tokens.fontUi
                                font.pixelSize: panel.tokens.sizeBody
                                font.strikeout: treeRow.completed
                                elide: Text.ElideRight
                                renderType: Text.NativeRendering
                            }

                            Text {
                                visible: treeRow.kind === "project" || treeRow.childCount > 0
                                text: treeRow.completedTasks + "/" + treeRow.totalTasks
                                color: panel.mutedColor
                                font.family: panel.tokens.fontUi
                                font.pixelSize: panel.tokens.sizeMeta
                                renderType: Text.NativeRendering
                            }

                            TreeIconButton {
                                objectName: "projectTreeAddChildButton"
                                visible: panel.selectedNodeId === treeRow.nodeId
                                iconSource: "qrc:/assets/icons/plus.svg"
                                accessibleName: "添加子项"
                                accessibleDescription: "在“" + treeRow.title + "”下创建子项"
                                onClicked: {
                                    panel.selectNode(treeRow.nodeId)
                                    panel.createChildForSelected()
                                }
                            }

                            TreeIconButton {
                                objectName: "projectTreeDeleteButton"
                                visible: panel.selectedNodeId === treeRow.nodeId
                                iconSource: "qrc:/assets/icons/trash.svg"
                                accessibleName: "删除节点"
                                accessibleDescription: "删除“" + treeRow.title + "”及其全部子项"
                                danger: true
                                onClicked: {
                                    panel.selectNode(treeRow.nodeId)
                                    panel.removeSelected()
                                }
                            }
                        }
                    }

                    Text {
                        objectName: "projectEmptyState"
                        anchors.centerIn: parent
                        width: Math.min(parent.width - panel.tokens.space6, 280)
                        visible: panel.model && panel.model.projectCount === 0
                        text: "还没有项目\n从右上角创建第一个项目，再在右侧补充背景与下一步。"
                        color: panel.mutedColor
                        font.family: panel.tokens.fontUi
                        font.pixelSize: panel.tokens.sizeBody
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        renderType: Text.NativeRendering
                        Accessible.role: Accessible.StaticText
                        Accessible.name: "项目树为空"
                        Accessible.description: text
                    }
                }
            }

            Rectangle {
                objectName: "projectInspectorCard"
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: panel.tokens.radiusMd
                color: panel.tokens.surface
                border.width: 1
                border.color: panel.tokens.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: panel.tokens.space4
                    spacing: panel.tokens.space3

                    TextField {
                        id: titleEditor
                        objectName: "projectInspectorTitleEditor"
                        Layout.fillWidth: true
                        Layout.preferredHeight: panel.tokens.primaryControlHeight
                        enabled: panel.selectedNodeId > 0
                        text: panel.selectedTitle
                        placeholderText: "选择左侧项目或任务"
                        selectByMouse: true
                        activeFocusOnTab: true
                        maximumLength: 256
                        font.pixelSize: panel.tokens.sizeHeading
                        font.weight: Font.DemiBold
                        font.family: panel.tokens.fontUi
                        color: panel.inkColor
                        renderType: Text.NativeRendering
                        Accessible.role: Accessible.EditableText
                        Accessible.name: "项目节点标题"
                        Accessible.description: enabled ? "编辑所选节点标题" : "请先在左侧选择项目或任务"
                        onAccepted: panel.commitInspectorTitle(text)
                        onEditingFinished: panel.commitInspectorTitle(text)
                        background: Rectangle {
                            radius: panel.tokens.radiusMd
                            color: titleEditor.enabled ? panel.tokens.surfaceHover
                                                       : panel.tokens.surfaceDisabled
                            border.width: titleEditor.activeFocus ? 2 : 1
                            border.color: titleEditor.activeFocus
                                          ? panel.tokens.focusRing : panel.tokens.border
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: panel.selectedNodeId > 0
                              ? (panel.selectedKind === "project" ? "项目" : "任务")
                                + " · " + panel.selectedProgressText
                              : "左侧组织结构，右侧记录背景、要求和下一步。"
                        color: panel.mutedColor
                        font.pixelSize: panel.tokens.sizeBody
                        font.family: panel.tokens.fontUi
                        elide: Text.ElideRight
                        renderType: Text.NativeRendering
                    }

                    ScrollView {
                        id: descriptionScroll
                        objectName: "projectDescriptionScroll"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 160
                        clip: true
                        activeFocusOnTab: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                        background: Rectangle {
                            radius: panel.tokens.radiusMd
                            color: descriptionEditor.enabled ? panel.tokens.surfaceHover
                                                             : panel.tokens.surfaceDisabled
                            border.width: descriptionEditor.activeFocus ? 2 : 1
                            border.color: descriptionEditor.activeFocus
                                          ? panel.tokens.focusRing : panel.tokens.border
                        }

                        TextArea {
                            id: descriptionEditor
                            objectName: "projectDescriptionEditor"
                            width: descriptionScroll.availableWidth
                            enabled: panel.selectedNodeId > 0
                            text: panel.selectedDescription
                            placeholderText: "记录背景、要求、风险与下一步处理"
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            activeFocusOnTab: true
                            font.pixelSize: panel.tokens.sizeBody
                            font.family: panel.tokens.fontUi
                            color: panel.inkColor
                            renderType: Text.NativeRendering
                            Accessible.role: Accessible.EditableText
                            Accessible.name: "项目节点详情"
                            Accessible.description: enabled ? "编辑所选节点的详细内容"
                                                               : "请先选择项目节点"
                            onActiveFocusChanged: if (!activeFocus) panel.commitDescription(text)
                            background: Rectangle { color: panel.tokens.transparent }
                        }
                    }

                    GridLayout {
                        objectName: "projectInspectorMetaBar"
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: panel.tokens.space4
                        rowSpacing: panel.tokens.space1

                        Text {
                            text: "路径"
                            color: panel.mutedColor
                            font.pixelSize: panel.tokens.sizeBody
                            font.family: panel.tokens.fontUi
                        }
                        Text {
                            objectName: "projectInspectorPath"
                            Layout.fillWidth: true
                            text: panel.selectedNodeId > 0 ? panel.selectedPath : "未选择"
                            color: panel.inkColor
                            font.pixelSize: panel.tokens.sizeBody
                            font.family: panel.tokens.fontUi
                            elide: Text.ElideMiddle
                            Accessible.role: Accessible.StaticText
                            Accessible.name: "节点路径"
                            Accessible.description: text
                        }
                        Text {
                            text: "类型"
                            color: panel.mutedColor
                            font.pixelSize: panel.tokens.sizeBody
                            font.family: panel.tokens.fontUi
                        }
                        Text {
                            text: panel.selectedKind === "project" ? "项目"
                                  : panel.selectedKind === "task" ? "任务" : "未选择"
                            color: panel.inkColor
                            font.pixelSize: panel.tokens.sizeBody
                            font.family: panel.tokens.fontUi
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: panel.tokens.space2

                        ToolPill {
                            objectName: "projectInspectorCompleteButton"
                            visible: panel.canToggleSelectedComplete
                            text: panel.selectedCompleted ? "标为未完成" : "标为完成"
                            widthHint: 120
                            projectStyle: true
                            theme: panel.tokens
                            accessibleDescription: "切换所选叶子任务的完成状态"
                            onClicked: panel.toggleSelectedComplete()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        objectName: "projectPersistenceError"
                        Layout.fillWidth: true
                        visible: panel.model && panel.model.lastError !== undefined &&
                                 panel.model.lastError.length > 0
                        text: panel.model && panel.model.lastError !== undefined
                              ? panel.model.lastError : ""
                        color: panel.tokens.danger
                        font.family: panel.tokens.fontUi
                        font.pixelSize: panel.tokens.sizeBody
                        wrapMode: Text.WordWrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }
                }
            }
        }
    }

    Connections {
        target: panel.model
        ignoreUnknownSignals: true

        function onSelectionChanged() {
            panel.clearTransientPressState()
            Qt.callLater(panel.syncEditors)
        }

        function onTreeChanged() {
            panel.clearTransientPressState()
            Qt.callLater(panel.syncEditors)
        }

        function onPersistenceError(message) {
            panel.noticeRequested(message)
        }
    }

    Component.onCompleted: Qt.callLater(syncEditors)
}
