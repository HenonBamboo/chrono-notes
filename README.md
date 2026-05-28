<p align="center">
  <img src="assets/chrono_notes_banner.svg" alt="ChronoNotes banner" width="100%">
</p>

# ChronoNotes

ChronoNotes 是一款基于 **Qt 6 Quick/QML + C++17** 的 Windows 桌面笔记与任务整理工具。它不试图变成笨重的项目管理平台，也不把你的日常记录默认丢进云端；它专注于本地、快速、按时间阶段组织的个人任务记录，并在需要时通过 OpenAI-compatible 接口生成阶段摘要。

## 核心定位

ChronoNotes 适合这类场景：

- 你想按“每天、每周、每月、每年”管理短任务和随手记录。
- 你希望数据默认保存在本机，而不是一上来就绑定账号和云同步。
- 你需要搜索、归档、导入导出、重复任务这些实用能力。
- 你希望 AI 只作为摘要助手，而不是把整个应用绑死在某个在线服务上。

它不适合拿来替代 Notion、Jira、Trello 这类协作系统。把一个轻量本地工具硬拧成团队平台，那就像拿便利贴管公司预算，热闹归热闹，迟早出事。

## 功能特性

- **时间阶段视图**：支持每天、每周、每月、每年四类视图。
- **本地优先存储**：笔记、配置、操作日志和摘要历史默认写入本地目录。
- **SQLite 数据层**：主数据使用 QtSql SQLite；旧 `notes.db.txt` 会在首次启动时迁移。
- **快速编辑**：支持新增、编辑、完成、取消完成、删除和短时 `Ctrl+Z` 撤销。
- **自动收纳**：周、月、年视图会聚合每日事项，并以只读归档形式展示。
- **全局搜索**：支持跨阶段搜索、完成状态筛选和关键词高亮。
- **重复任务**：支持 `daily`、`weekly`、`monthly`、`yearly` 重复规则。
- **导入导出**：支持 JSON 导入/导出和 Markdown 导出。
- **AI 摘要**：支持配置 API URL、API Key 和模型名，调用 OpenAI-compatible HTTP 接口异步生成摘要。
- **项目树视图**：提供本地项目结构视图，用于梳理构建与拆分关系。
- **原生桌面体验**：Qt/QML 无边框窗口，支持四边与四角拖拽缩放。

## 技术栈

| 模块 | 实现 |
| --- | --- |
| UI | Qt 6 Quick/QML，不使用 WebView |
| 应用层 | C++17 + Qt 模型，入口位于 `src/qt_main.cpp` |
| 数据层 | `src/note_store.cpp` / `src/note_store.h`，QtSql SQLite |
| 配置 | `src/config.cpp` / `src/config.h`，本地 ini 文件 |
| AI 客户端 | `src/ai_client.cpp` / `src/ai_client.h`，OpenAI-compatible HTTP |
| 导入导出 | `src/backup_service.cpp` / `src/backup_service.h` |
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
- 支持 C++17 的编译器

仓库默认按这个相对路径查找 Qt：

```text
third_party/Qt/6.8.3/mingw_64
```

如果你的 Qt 安装在其他位置，可以在配置 CMake 时传入自己的 `CMAKE_PREFIX_PATH`。不要把本机 Qt SDK 提交进仓库，那个体积不是依赖，是行李箱。

## 构建

下面示例使用通用占位路径，请把 `<qt-mingw-path>` 换成你自己的 Qt MinGW 安装目录。

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="<qt-mingw-path>"
cmake --build build -j 6
```

CMake 目标：

```text
ChronoNotes
```

构建后的主程序通常位于：

```text
build\ChronoNotes.exe
```

如果你使用 CLion，也可以直接打开项目根目录，由 CLion 管理 CMake 配置和构建目录。

## 验证

运行完整测试：

```powershell
ctest --test-dir build --output-on-failure
```

运行 QML 静态检查：

```powershell
qmllint `
  qml/Main.qml `
  qml/AppTitleBar.qml `
  qml/StageTabs.qml `
  qml/SearchBar.qml `
  qml/EventComposer.qml `
  qml/ProgressStats.qml `
  qml/NoteListPanel.qml `
  qml/NoteRow.qml `
  qml/OverlayPanel.qml `
  qml/AiSummaryPanel.qml `
  qml/DetailPanel.qml `
  qml/SettingsPanel.qml `
  qml/TinyMeta.qml `
  qml/ToolPill.qml `
  qml/WindowButton.qml
```

如果 `cmake`、`ctest` 或 `qmllint` 不在 PATH 中，请使用你本机 Qt/CMake 安装目录下的对应可执行文件。README 不写死个人路径，省得别人 clone 下来第一眼就像在参观你家硬盘。

## 打包

发布包脚本会把可执行文件、Qt 运行时、README 和展示素材打进 ZIP。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/package_release.ps1 -BuildDir build -OutputDir dist
```

输出位置：

```text
dist\ChronoNotes.zip
```

## 数据文件

运行数据默认保存在程序目录下的 `data/`：

```text
data\notes.sqlite
data\operations.jsonl
data\summary-history.md
data\config.ini
```

测试会通过 `STICKY_NOTES_DATA_DIR` 指向临时目录，避免污染真实运行数据。这个环境变量名称保留旧前缀是为了兼容现有测试和历史数据路径，不代表项目展示名。

## AI 配置

ChronoNotes 使用 OpenAI-compatible HTTP 接口。你需要在设置面板里配置：

- API URL
- API Key
- 模型名称

项目不会内置公开 API Key。把 Key 写进仓库这种事，属于把门钥匙贴门口，还挺自信，别这么干。

## 许可

本项目使用 `PolyForm Noncommercial License 1.0.0`。你可以在非商业目的下使用、学习和修改本项目；商业使用不在该许可范围内。

## 当前状态

ChronoNotes 已完成 Qt/QML 主线、SQLite 存储、导入导出、搜索、重复任务、AI 摘要、摘要历史、操作日志和发布包脚本。后续更值得投入的是操作日志恢复界面、视觉细节打磨和正式安装器。
