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

        function updateParentStats(parentId) {
            const parentIndex = indexOfNode(parentId)
            if (parentIndex < 0)
                return
            let childCount = 0
            let total = 0
            let done = 0
            for (let i = 0; i < count; ++i) {
                const child = get(i)
                if (child.parentId !== parentId)
                    continue
                childCount += 1
                total += child.totalTasks
                done += child.completedTasks
            }
            setProperty(parentIndex, "childCount", childCount)
            setProperty(parentIndex, "totalTasks", total)
            setProperty(parentIndex, "completedTasks", done)
            setProperty(parentIndex, "completed", total > 0 && done === total)
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
            const parent = parentIndex >= 0 ? get(parentIndex) : ({ depth: 0 })
            totalTasks += 1
            if (parentIndex >= 0)
                setProperty(parentIndex, "expanded", true)
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
            updateParentStats(parentId)
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
            if (index < 0 || get(index).kind !== "task" || get(index).childCount > 0)
                return false
            const completed = !get(index).completed
            setProperty(index, "completed", completed)
            setProperty(index, "completedTasks", completed ? 1 : 0)
            completedTasks += completed ? 1 : -1
            updateParentStats(get(index).parentId)
            return true
        }

        function removeNode(nodeId) {
            for (let i = count - 1; i >= 0; --i) {
                const node = get(i)
                if (node.nodeId === nodeId || node.parentId === nodeId) {
                    if (node.kind === "project")
                        projectCount -= 1
                    if (node.kind === "task") {
                        totalTasks -= Math.max(node.totalTasks, 1)
                        completedTasks -= node.completedTasks
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
        panel.activeCheckNodeId = 0
        noticeSpy.clear()
    }

    function createProject(title) {
        const id = panel.createRootProject()
        panel.commitInspectorTitle(title)
        return id
    }

    function createChild(title) {
        const id = panel.createChildForSelected()
        panel.commitInspectorTitle(title)
        return id
    }

    function test_treeUsesReadableChineseAndLeftDoesNotInlineEdit() {
        compare(findChild(panel, "projectCreateRootButton").text, "+ 项目")
        compare(findChild(panel, "projectInspectorTitleEditor").placeholderText, "选择左侧项目或任务")

        const projectId = panel.createRootProject()
        verify(projectId > 0)
        compare(panel.editingNodeId, 0)
        compare(findChild(panel, "projectTreeInlineTitleEditor"), null)

        panel.commitInspectorTitle("产品发布")
        compare(projectModel.get(0).title, "产品发布")
    }

    function test_treeAddAndDeleteActionsUseMatchedIconButtons() {
        createProject("产品发布")

        const addButton = findChild(panel, "projectTreeAddChildButton")
        const deleteButton = findChild(panel, "projectTreeDeleteButton")
        verify(addButton !== null)
        verify(deleteButton !== null)
        compare(addButton.text, "+")
        compare(deleteButton.text, "-")
        compare(addButton.width, deleteButton.width)
        compare(addButton.height, deleteButton.height)
    }

    function test_rightWorkbenchEditsTitleAndDescription() {
        createProject("产品发布")
        const titleEditor = findChild(panel, "projectInspectorTitleEditor")
        const descriptionEditor = findChild(panel, "projectDescriptionEditor")

        verify(titleEditor !== null)
        verify(descriptionEditor !== null)
        compare(titleEditor.text, "产品发布")

        panel.commitInspectorTitle("发布计划")
        panel.commitDescription("记录这个项目的背景、要求、下一步处理")

        compare(projectModel.get(0).title, "发布计划")
        compare(projectModel.get(0).description, "记录这个项目的背景、要求、下一步处理")
        verify(panel.aiContextText.indexOf("记录这个项目") >= 0)
    }

    function test_completionActionOnlyAppearsForLeafTasks() {
        createProject("产品发布")
        compare(panel.canToggleSelectedComplete, false)

        const parentTask = createChild("父任务")
        createChild("子任务")
        panel.setSelection(parentTask, "父任务", "task", 1, 1, 0, false, 1, "")
        wait(0)
        compare(panel.canToggleSelectedComplete, false)

        const leafId = projectModel.addChild(projectModel.get(0).nodeId, "叶子任务")
        panel.setSelection(leafId, "叶子任务", "task", 0, 1, 0, false, 1, "")
        wait(0)
        compare(panel.selectedKind, "task")
        compare(panel.selectedChildCount, 0)
        compare(panel.canToggleSelectedComplete, true)

        panel.toggleSelectedComplete()
        compare(projectModel.get(projectModel.indexOfNode(leafId)).completed, true)
    }

    function test_rightWorkbenchDescriptionFillsAvailableSpaceAndMetaStaysBelow() {
        createProject("产品发布")

        const descriptionScroll = findChild(panel, "projectDescriptionScroll")
        const metaBar = findChild(panel, "projectInspectorMetaBar")
        verify(descriptionScroll !== null)
        verify(metaBar !== null)
        compare(descriptionScroll.Layout.fillHeight, true)
        verify(descriptionScroll.height > metaBar.height)

        panel.width = 900
        panel.height = 620
        wait(0)
        verify(descriptionScroll.height > 180)
    }

    function test_clickingElsewhereClearsCheckboxPressedStateAndSelectionChangesOnSingleClick() {
        createProject("产品发布")
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

        const projectId = createProject("产品发布")
        panel.commitDescription("右侧具体内容")
        verify(panel.aiContextText.indexOf("右侧具体内容") >= 0)

        panel.clearSelection()
        compare(panel.aiSummaryAvailable, true)
        verify(panel.aiContextText.indexOf("整个项目树") >= 0)
        verify(projectId > 0)
    }
}
