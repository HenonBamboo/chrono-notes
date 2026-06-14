# ChronoNotes v0.1.0

## Highlights

- Reworked the project tree into a clearer structure-and-workbench flow: the left pane owns hierarchy, title editing, add/remove actions, and compact progress; the right pane owns the selected item's title, long description, completion state, and metadata.
- Hardened project progress logic: parent tasks and projects now derive completion from child tasks; leaf tasks count themselves only when they have no children.
- Added persistent project/task descriptions with backward-compatible JSON loading.
- Adapted AI summary and settings panels to the active workspace, with clearer sticky-note data wording and less intrusive drawer styling.
- Renamed user-facing note copy to sticky-note language while keeping internal `Note*` code names stable.

## Stability And Safety

- Empty titles are handled without corrupting existing nodes.
- Long descriptions remain editable through a scrollable text area.
- Parent nodes with children no longer expose misleading manual completion toggles.
- Project-tree summary context now includes title, path, progress, description, and unfinished child information.

## Verification

- `project_tree_model_tests`
- QML interaction tests for app title bar, project tree, settings, AI summary, note list, and search
- Full QML suite: `40 passed, 0 failed`

## Open Design

Open Design MCP was attempted before implementation, but the local daemon was unreachable at `http://127.0.0.1:7456`. The implementation therefore used the existing local Open Design project specification as the fallback design source.
