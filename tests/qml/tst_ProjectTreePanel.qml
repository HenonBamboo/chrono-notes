import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "ProjectTreePanel"
    when: windowShown

    ListModel {
        id: projectModel

        property int projectCount: 0
        property int completedTasks: 0
        property int totalTasks: 0
        property int nextId: 10

        function indexOfNode(nodeId) {
            for (let i = 0; i < count; ++i) {
                if (get(i).nodeId === nodeId)
                    return i
            }
            return -1
        }

        function addProject(title) {
            const id = nextId++
            projectCount += 1
            append({
                nodeId: id,
                parentId: 0,
                title,
                kind: "project",
                description: "",
                completed: false,
                depth: 0,
                expanded: true,
                childCount: 0,
                totalTasks: 0,
                completedTasks: 0
            })
            return id
        }

        function addChild(parentId, title) {
            const id = nextId++
            const parentIndex = indexOfNode(parentId)
            const parent = parentIndex >= 0 ? get(parentIndex) : ({ depth: 0, childCount: 0, totalTasks: 0 })
            totalTasks += 1
            if (parentIndex >= 0) {
                setProperty(parentIndex, "expanded", true)
                setProperty(parentIndex, "childCount", parent.childCount + 1)
                setProperty(parentIndex, "totalTasks", parent.totalTasks + 1)
            }
            append({
                nodeId: id,
                parentId,
                title,
                kind: "task",
                description: "",
                completed: false,
                depth: parent.depth + 1,
                expanded: true,
                childCount: 0,
                totalTasks: 1,
                completedTasks: 0
            })
            return id
        }

        function updateTitle(nodeId, title) {
            const index = indexOfNode(nodeId)
            if (index >= 0)
                setProperty(index, "title", title)
        }

        function updateDescription(nodeId, description) {
            const index = indexOfNode(nodeId)
            if (index >= 0)
                setProperty(index, "description", description)
        }

        function toggleExpanded(nodeId) {
            const index = indexOfNode(nodeId)
            if (index >= 0)
                setProperty(index, "expanded", !get(index).expanded)
        }

        function toggleComplete(nodeId) {
            const index = indexOfNode(nodeId)
            if (index < 0 || get(index).kind !== "task")
                return
            const completed = !get(index).completed
            setProperty(index, "completed", completed)
            completedTasks += completed ? 1 : -1
        }

        function removeNode(nodeId) {
            for (let i = count - 1; i >= 0; --i) {
                const node = get(i)
                if (node.nodeId === nodeId || node.parentId === nodeId) {
                    if (node.kind === "project")
                        projectCount -= 1
                    if (node.kind === "task") {
                        totalTasks -= 1
                        if (node.completed)
                            completedTasks -= 1
                    }
                    remove(i)
                }
            }
        }
    }

    ProjectTreePanel {
        id: panel
        width: 700
        height: 520
        model: projectModel
    }

    SignalSpy {
        id: noticeSpy
        target: panel
        signalName: "noticeRequested"
    }

    function init() {
        projectModel.clear()
        projectModel.projectCount = 0
        projectModel.completedTasks = 0
        projectModel.totalTasks = 0
        projectModel.nextId = 10
        panel.clearSelection()
        panel.noticeText = ""
        panel.editingNodeId = 0
        panel.pendingCreatedNodeId = 0
        panel.activeCheckNodeId = 0
        noticeSpy.clear()
    }

    function createProject(title) {
        const id = panel.createRootProject()
        panel.commitEditing(title)
        return id
    }

    function createChild(title) {
        const id = panel.createChildForSelected()
        panel.commitEditing(title)
        return id
    }

    function test_treeUsesReadableChineseAndInspectorHasNoDuplicateActions() {
        compare(findChild(panel, "projectCreateRootButton").text, "+ 项目")
        compare(findChild(panel, "projectInspectorTitleEditor").placeholderText, "选择左侧项目或任务")
        verify(findChild(panel, "projectAddChildButton") === null)
        verify(findChild(panel, "projectInspectorRenameButton") === null)
        verify(findChild(panel, "projectChildTaskList") === null)
    }

    function test_rootAndDeepChildrenAreCreatedAndEditedFromLeftTree() {
        let parentId = createProject("产品改版")
        for (let depth = 1; depth <= 8; ++depth) {
            parentId = createChild("第" + depth + "层任务")
        }

        compare(projectModel.count, 9)
        compare(projectModel.get(8).depth, 8)
        verify(panel.indentForDepth(1) > panel.indentForDepth(0))
        verify(panel.indentForDepth(8) > panel.indentForDepth(3))
        verify(panel.indentForDepth(12) <= panel.indentForDepth(8) + 24)
    }

    function test_rightWorkbenchEditsTitleAndDescription() {
        createProject("产品改版")
        const titleEditor = findChild(panel, "projectInspectorTitleEditor")
        const descriptionEditor = findChild(panel, "projectDescriptionEditor")

        verify(titleEditor !== null)
        verify(descriptionEditor !== null)
        compare(titleEditor.text, "产品改版")

        panel.commitInspectorTitle("发布计划")
        panel.commitDescription("记录这个项目的背景、要求、下一步处理")

        compare(projectModel.get(0).title, "发布计划")
        compare(projectModel.get(0).description, "记录这个项目的背景、要求、下一步处理")
        verify(panel.aiContextText.indexOf("记录这个项目的背景") >= 0)
    }

    function test_rightWorkbenchHasSingleCompletionControlForTasksOnly() {
        createProject("产品改版")
        const taskId = createChild("验收清单")

        const completeButton = findChild(panel, "projectInspectorCompleteButton")
        verify(completeButton !== null)
        compare(completeButton.enabled, true)

        panel.toggleSelectedComplete()
        compare(projectModel.get(projectModel.indexOfNode(taskId)).completed, true)
        compare(panel.activeCheckNodeId, 0)

        panel.setSelection(projectModel.get(0).nodeId, projectModel.get(0).title, "project", 1, 1, 1, false, 0, projectModel.get(0).description)
        compare(completeButton.enabled, false)
    }

    function test_parentTaskWithChildrenShowsRollupAndCannotBeCompletedDirectly() {
        createProject("产品改版")
        const parentTask = createChild("父任务")
        createChild("子任务")

        panel.setSelection(parentTask, "父任务", "task", 1, 1, 0, false, 1, "")

        const completeButton = findChild(panel, "projectInspectorCompleteButton")
        verify(completeButton !== null)
        compare(completeButton.enabled, false)
        verify(completeButton.text.indexOf("子项") >= 0 || completeButton.text.indexOf("汇总") >= 0)
    }

    function test_treeDeleteActionUsesMinusAndDescriptionHasScrollContainer() {
        createProject("产品改版")

        const deleteButton = findChild(panel, "projectTreeDeleteButton")
        const descriptionScroll = findChild(panel, "projectDescriptionScroll")

        verify(deleteButton !== null)
        compare(deleteButton.text, "-")
        verify(descriptionScroll !== null)
    }

    function test_clickingElsewhereClearsCheckboxPressedStateAndSelectionChangesOnSingleClick() {
        createProject("产品改版")
        const firstTask = createChild("验收清单")
        const secondTask = createChild("发布说明")

        panel.markCheckPressed(firstTask)
        compare(panel.activeCheckNodeId, firstTask)

        panel.setSelection(secondTask, "发布说明", "task", 0, 1, 0, false, 2, "")
        compare(panel.selectedNodeId, secondTask)
        compare(panel.activeCheckNodeId, 0)

        panel.clearTransientPressState()
        compare(panel.activeCheckNodeId, 0)
    }

    function test_summaryContextIncludesDescriptionAndNoSelectionUsesTreeOverview() {
        compare(panel.aiSummaryAvailable, false)
        compare(panel.aiContextText, "")

        const projectId = createProject("产品改版")
        panel.commitDescription("右侧具体内容")
        verify(panel.aiContextText.indexOf("右侧具体内容") >= 0)

        panel.clearSelection()
        compare(panel.aiSummaryAvailable, true)
        verify(panel.aiContextText.indexOf("整个项目树") >= 0)
        verify(projectId > 0)
    }
}
