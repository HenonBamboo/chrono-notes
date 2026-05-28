<p align="center">
  <img src="assets/chrono_notes_banner.svg" alt="ChronoNotes banner" width="100%">
</p>

# ChronoNotes

ChronoNotes 是一款基于 **Qt 6 Quick/QML + C++17** 的 Windows 桌面笔记与任务整理工具。它不是 Notion 的替代品，也不是云协作系统；它专注于一个更窄、更扎实的目标：让本地日常事项可以按时间阶段组织、快速输入、可靠保存，并在需要时调用 OpenAI-compatible 接口生成摘要。

## 为什么做它

普通便签适合随手记，但时间维度经常散；完整项目管理工具又太重，打开就像要开会。ChronoNotes 走中间路线：用“每天、每周、每月、每年”的阶段视图承载短任务，用本地 SQLite 保证数据在自己机器上，用 AI 摘要把当前阶段内容压成可回顾的记录。

## 功能特性

- 时间阶段：支持每天、每周、每月、每年四类视图。
- 本地优先：笔记、配置、操作日志和摘要历史默认保存在本地。
- SQLite 存储：主数据使用 QtSql SQLite，旧 `notes.db.txt` 会在首次启动时迁移。
- 快速编辑：支持新增、编辑、完成、取消完成、删除和 `Ctrl+Z` 短时撤销。
- 自动收纳：周、月、年视图会聚合每日事项，并以只读归档形式展示。
- 全局搜索：支持跨阶段搜索、完成状态筛选和关键词高亮。
- 重复任务：支持 daily、weekly、monthly、yearly 重复规则。
- 导入导出：支持 JSON 导入/导出和 Markdown 导出。
- AI 摘要：支持配置 API URL、API Key 和模型名，调用 OpenAI-compatible HTTP 接口异步生成摘要。
- 项目树：提供本地项目结构视图，用于梳理构建和分化关系。
- 无边框窗口：支持自绘标题栏、四边和四角拖拽缩放。

## 技术路线

| 模块 | 实现 |
| --- | --- |
| UI | Qt 6 Quick/QML，不使用 WebView |
| 应用层 | C++17 + Qt 模型，入口位于 `src/qt_main.cpp` |
| 数据层 | `src/note_store.cpp` / `src/note_store.h`，QtSql SQLite |
| 配置 | `src/config.cpp` / `src/config.h`，本地 ini 文件 |
| AI 客户端 | `src/ai_client.cpp` / `src/ai_client.h`，OpenAI-compatible HTTP |
| 备份导入导出 | `src/backup_service.cpp` / `src/backup_service.h` |
| 测试 | C++ 单元测试 + QML 交互测试，统一接入 CTest |

## 项目结构

```text
assets/     图标与 README 展示素材
docs/       项目状态与开发说明
qml/        Qt Quick/QML 界面组件
src/        C++ 应用层、数据层、配置和 AI 客户端
tests/      C++ 与 QML 测试
tools/      打包与维护脚本
```

## 构建要求

- Windows
- Qt 6.8.x MinGW 版本
- CMake 4.0+
- C++17 编译器
- CLion 可选，但当前工程按 CLion + CMake 路线维护

当前仓库默认查找 Qt 路径：

```text
third_party/Qt/6.8.3/mingw_64
```

如果你的 Qt 安装在其他位置，需要调整 `CMAKE_PREFIX_PATH` 或在 CMake 配置阶段传入自己的 Qt 路径。别把本地 Qt SDK 提交进仓库，那个体积不是仓库，是搬家。

## 构建

```powershell
$env:PATH='C:\Program Files\JetBrains\CLion 2025.2\bin\mingw\bin;' + $env:PATH
& 'C:\Program Files\JetBrains\CLion 2025.2\bin\cmake\win\x64\bin\cmake.exe' --build 'D:\AI\Codex\cmake-build-qt-debug' -j 6
```

CMake 目标：

```text
ChronoNotes
```

构建后的主程序：

```text
cmake-build-qt-debug\ChronoNotes.exe
```

## 验证

运行完整测试：

```powershell
& 'C:\Program Files\JetBrains\CLion 2025.2\bin\cmake\win\x64\bin\ctest.exe' --test-dir 'D:\AI\Codex\cmake-build-qt-debug' --output-on-failure
```

运行 QML 静态检查：

```powershell
& 'D:\AI\Codex\third_party\Qt\6.8.3\mingw_64\bin\qmllint.exe' 'D:\AI\Codex\qml\Main.qml' 'D:\AI\Codex\qml\AppTitleBar.qml' 'D:\AI\Codex\qml\StageTabs.qml' 'D:\AI\Codex\qml\SearchBar.qml' 'D:\AI\Codex\qml\EventComposer.qml' 'D:\AI\Codex\qml\ProgressStats.qml' 'D:\AI\Codex\qml\NoteListPanel.qml' 'D:\AI\Codex\qml\NoteRow.qml' 'D:\AI\Codex\qml\OverlayPanel.qml' 'D:\AI\Codex\qml\AiSummaryPanel.qml' 'D:\AI\Codex\qml\DetailPanel.qml' 'D:\AI\Codex\qml\SettingsPanel.qml' 'D:\AI\Codex\qml\TinyMeta.qml' 'D:\AI\Codex\qml\ToolPill.qml' 'D:\AI\Codex\qml\WindowButton.qml'
```

## 打包

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'D:\AI\Codex\tools\package_release.ps1'
```

输出位置：

```text
D:\AI\Codex\dist\ChronoNotes.zip
```

## 数据文件

运行数据默认保存在构建输出目录下：

```text
cmake-build-qt-debug\data\notes.sqlite
cmake-build-qt-debug\data\operations.jsonl
cmake-build-qt-debug\data\summary-history.md
cmake-build-qt-debug\data\config.ini
```

测试会通过 `STICKY_NOTES_DATA_DIR` 指向临时目录，避免污染真实运行数据。这个环境变量名称保留旧前缀是为了兼容现有测试和历史数据路径，不代表项目展示名。

## 许可

本项目使用 `PolyForm Noncommercial License 1.0.0`。你可以在非商业目的下使用、学习和修改本项目；商业使用不在该许可范围内。

## 当前状态

ChronoNotes 已完成 Qt/QML 主线、SQLite 存储、导入导出、搜索、重复任务、AI 摘要、摘要历史、操作日志和发布包脚本。后续更值得投入的是操作日志恢复界面、视觉细节打磨和正式安装器，而不是一上来搞账号系统或云同步。那个方向不是不能做，是现在做就像房子地基还没验收先装水晶吊灯，挺热闹，也挺悬。
