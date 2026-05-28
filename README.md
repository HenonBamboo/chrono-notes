# StickyNotesC

这是一个 Windows 桌面便签程序。当前主线是 **Qt 6 Quick/QML + C++17**，可在 CLion 中通过 CMake 管理、编译和运行。

## 当前技术路线

- UI：Qt 6 Quick/QML，不使用 WebView，也不是浏览器页面。
- 应用层：C++17，入口位于 `src/qt_main.cpp`。
- Qt 模型：`src/qt_note_app.cpp` / `src/qt_note_app.h`。
- 数据存储：C++17 + QtSql SQLite，位于 `src/note_store.cpp` / `src/note_store.h`，首次启动会从旧 `notes.db.txt` 自动迁移。
- 配置存储：C++17 本地 ini 文件，位于 `src/config.cpp` / `src/config.h`。
- AI 接口：C++17 + Qt Network，OpenAI-compatible HTTP 接口，位于 `src/ai_client.cpp` / `src/ai_client.h`。

## 当前功能

- 按每天、每周、每月、每年管理便签。
- 每条事件可标记完成，完成后记录完成时间。
- 未完成事件显示在已完成事件上方，各组内部按最近时间排序。
- 周、月、年视图会自动收纳每日事件，并以只读内容显示。
- 自动收纳内容在周、月、年视图中统一放入“自动收纳”分组，来源日期显示在行内信息里。
- 自动收纳分组可在列表里折叠或展开，避免历史内容挤占手动计划。
- 支持点击事件后在列表内直接编辑。
- 支持再次点击“全选完成”取消全选。
- 删除事件、完成/取消完成事件后，支持 5 秒内按 `Ctrl+Z` 撤销。
- 支持 `Ctrl+F` 呼出全局搜索，按关键词搜索全部阶段和自动收纳内容。
- 搜索支持按全部、未完成、已完成筛选，并在结果中高亮关键词。
- 支持 `Ctrl+N` 聚焦新建输入框。
- 支持分层 `Esc` 行为：先退出编辑焦点，再关闭抽屉或搜索。
- 支持长内容右侧详情抽屉，普通事件可编辑，自动收纳事件只读查看。
- 支持重复任务：每天、每周、每月、每年，完成后自动生成下一条。
- 支持通过系统文件选择窗口导出 JSON、导入 JSON、导出 Markdown。
- 支持清理当前阶段已完成、全局已完成、当前阶段全部、全部便签。
- 支持本地操作日志，记录新增、更新、完成、删除、撤销、导入导出、清理等动作。
- 支持 AI 摘要历史保存，便于回看每次摘要要求、结果和输入内容。
- 支持配置 API URL、API Key、模型名称，并对当前视图内容生成 AI 摘要。
- 支持无边框窗口、四边与四角拖拽缩放。

## 构建目标

CMake 目标：

```text
StickyNotesC
```

CLion 中选择并运行 `StickyNotesC` 即可。

推荐构建目录：

```text
D:\AI\Codex\cmake-build-qt-debug
```

## 常用验证命令

构建：

```powershell
$env:PATH='C:\Program Files\JetBrains\CLion 2025.2\bin\mingw\bin;' + $env:PATH
& 'C:\Program Files\JetBrains\CLion 2025.2\bin\cmake\win\x64\bin\cmake.exe' --build 'D:\AI\Codex\cmake-build-qt-debug' -j 6
```

单元测试：

```powershell
& 'C:\Program Files\JetBrains\CLion 2025.2\bin\cmake\win\x64\bin\ctest.exe' --test-dir 'D:\AI\Codex\cmake-build-qt-debug' --output-on-failure
```

QML 检查：

```powershell
$env:PATH='C:\Program Files\JetBrains\CLion 2025.2\bin\mingw\bin;' + $env:PATH
& 'D:\AI\Codex\third_party\Qt\6.8.3\mingw_64\bin\qmllint.exe' 'D:\AI\Codex\qml\Main.qml' 'D:\AI\Codex\qml\AppTitleBar.qml' 'D:\AI\Codex\qml\StageTabs.qml' 'D:\AI\Codex\qml\SearchBar.qml' 'D:\AI\Codex\qml\EventComposer.qml' 'D:\AI\Codex\qml\ProgressStats.qml' 'D:\AI\Codex\qml\NoteListPanel.qml' 'D:\AI\Codex\qml\NoteRow.qml' 'D:\AI\Codex\qml\OverlayPanel.qml' 'D:\AI\Codex\qml\AiSummaryPanel.qml' 'D:\AI\Codex\qml\DetailPanel.qml' 'D:\AI\Codex\qml\SettingsPanel.qml' 'D:\AI\Codex\qml\TinyMeta.qml' 'D:\AI\Codex\qml\ToolPill.qml' 'D:\AI\Codex\qml\WindowButton.qml'
```

发布 ZIP 包：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'D:\AI\Codex\tools\package_release.ps1'
```

输出位置：

```text
D:\AI\Codex\dist\StickyNotesC.zip
```

## 数据文件

运行后的数据默认保存在构建输出目录：

```text
cmake-build-qt-debug\data\notes.sqlite
cmake-build-qt-debug\data\operations.jsonl
cmake-build-qt-debug\data\summary-history.md
cmake-build-qt-debug\data\config.ini
```

Qt 测试会使用 `STICKY_NOTES_DATA_DIR` 指向临时目录，避免污染真实运行数据。

## 项目目录

- `src/`：当前主线 C++ 源码。
- `qml/`：当前主线 Qt/QML UI。
- `assets/`：程序图标和 UI 资源。
- `tests/`：C 数据层、配置层和 Qt 应用层测试。
- `tools/`：打包与维护脚本。

## 路线状态

已完成或基本完成：

- 工程主线收敛到 Qt/QML。
- README 已同步为当前 Qt/QML 路线。
- AI 摘要已改为后台异步调用，避免阻塞主界面。
- AI 摘要和设置抽屉已拆成独立 QML 组件。
- 完成/删除后的模型更新已减少整页刷新。
- 删除、完成/取消完成后支持 5 秒内撤销。
- Qt 测试使用临时数据目录。
- 全局搜索支持跨阶段检索，并可继续操作原事件。
- 长内容详情抽屉已接入，解决长文本强塞列表导致的阅读和编辑问题。
- `Ctrl+N` 和分层 `Esc` 已补齐，日常键盘流转不再一按就把抽屉和搜索全清空。
- 自动收纳项已收敛到一个外层分组，来源日期放在行内信息里，避免列表外层被日期切碎。
- 自动收纳分组已支持折叠/展开。
- 活动源码已从 C/C++ 混合切到 C++17。
- SQLite 数据层、旧文本数据迁移、动态内存数据模型、重复任务、带文件选择的 JSON/Markdown 导入导出、搜索筛选与关键词高亮已接入。
- 本地操作日志、AI 摘要历史、自动收纳默认密度控制已接入。
- ZIP 发布包脚本已接入，构建后可直接生成包含 Qt 运行时的分发包。

后续优先级建议：

1. 操作日志恢复界面：把 `operations.jsonl` 从审计记录进一步做成可视化恢复入口。
2. 视觉和动画继续统一打磨。
