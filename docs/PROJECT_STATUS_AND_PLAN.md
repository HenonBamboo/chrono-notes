# ChronoNotes 生产架构与维护说明

## 产品边界

ChronoNotes 1.0 面向 Windows 11，定位为本地优先、便签为主、项目为辅的
个人桌面工具。当前范围不包含账号、云同步、协作、日历、提醒、自动更新、
遥测和暗色主题。

设计目标按优先级排序：

1. 日常输入和回顾足够快。
2. 数据错误必须可见、可回滚、可恢复。
3. UI 层级清楚、克制且可通过键盘和辅助技术使用。
4. 非必要模块按需加载，长期维持轻量。

## 核心模块

### Notebook

`src/notebook.*` 是便签生命周期的深模块。公开接口只暴露：

- `QString` 值对象。
- 查询条件。
- `NotebookOutcome` / `NotebookMutation` 明确结果。
- 不依赖系统时间的显式时间戳入口。

SQLite、旧 `NoteStore`、排序容器、schema 和迁移实现不得泄漏到 `NoteApp`
或 QML。`NoteApp` 不允许包含 SQL、固定字符数组或 `NoteStore` 调用。

关键不变量：

- schema 当前版本为 2。
- 便签正文最多 65,536 个字符，超限失败且不改变内存或数据库状态。
- 重复任务具有非空稳定 `series_id`。
- 完成重复任务、派生下一项与撤销逆操作使用同一事务边界。
- UPSERT/DELETE 按便签执行，不允许事务外清表或每次整库重写。
- 迁移前创建旁路备份，失败时保留原数据库。

### Config 与 CredentialStore

`src/config.*` 使用 `QString` 值类型、版本字段与 `QSaveFile`。加载未来版本时
返回明确错误，调用方不得覆盖原文件。工作区导入只接受有界、单行、类型正确的
非敏感偏好。

API Key：

- 生产环境只进入 Windows Credential Manager。
- `config.ini` 中的旧 `api_key=` 只用于一次性迁移。
- 成功迁移后立即原子重写配置，移除明文字段。
- QML 只能读取“是否已配置”，不能读取密钥内容。

### AI Client

`src/ai_client.*` 使用不可变 `AiClientRequest` 值对象，结果通过
`AiClientResult` 返回。

安全规则：

- 默认仅允许 HTTPS。
- 仅在用户显式开启时允许 loopback HTTP。
- URL 不允许嵌入用户名或密码。
- 默认超时 45 秒，取消操作会中止 `QNetworkReply`。
- 请求和响应均有大小上限。
- 服务端错误会脱敏 API Key、用户要求和上下文。

`NoteApp` 维护递增 request id。新请求开始后，旧请求即使更晚完成也不能写入
状态或摘要历史。

### WorkspaceRecovery

`.chrononotes` v2 包含：

- 便签快照。
- 项目树快照。
- 摘要历史。
- 非敏感偏好。

永不包含 API Key、操作日志或诊断正文。

导入顺序固定为：

1. 完整读取。
2. schema、字段、引用与大小校验。
3. 生成影响预览和摘要。
4. 校验预览对应的文件摘要，防止预览后文件被替换。
5. 创建危险操作前备份。
6. 一次应用便签、偏好和摘要历史。
7. 任一步失败时执行逆提交。

保留策略为 7 个每日备份、3 个危险操作前备份，总量上限 200 MiB。
诊断日志上限 50 MiB。

### ProjectTreeModel

项目树持久化前后都必须校验：

- ID 唯一且为正数。
- 父节点存在。
- 不存在循环。
- 不存在孤儿。
- 标题、描述和深度有界。

选择状态由 `ProjectTreeModel` 单一持有，QML 只做绑定。任务、项目和已完成统计
使用缓存，避免每次 UI 刷新递归扫描整棵树。保存使用 `QSaveFile`。

### LocalProfile

数据路径优先级：

1. `STICKY_NOTES_DATA_DIR` 测试/开发覆盖。
2. `portable.flag` 对应的程序旁 `data/`。
3. `QStandardPaths::AppLocalDataLocation`。

旧程序旁数据只能复制迁移。验证新目录成功前不能切换，迁移完成后也不能删除旧文件。

### Clock

所有日期阶段、重复任务和撤销过期逻辑通过 `Clock` 获取时间：

- 生产使用 `SystemClock`。
- 测试使用可推进的 `TestClock`。

窗口恢复、系统休眠恢复和定时器触发都调用同一日期刷新入口。

## UI 结构

首页从上到下为：

1. 原生 Windows 11 标题栏。
2. 紧凑工作区切换与全局操作。
3. 低权重阶段导航。
4. 快速输入。
5. 轻量进度。
6. 便签列表。

项目树、AI 与设置使用 `Loader` 按需创建。原生标题栏用于保证 Snap、
系统菜单和辅助技术兼容性；内容区不再重复应用 Logo 和标题。

视觉令牌集中在 `qml/ChronoTokens.qml`。业务组件不允许出现裸十六进制颜色。
所有交互控件必须具备：

- `Accessible.name`。
- 必要时的 `Accessible.description`。
- 键盘焦点目标。
- 清晰焦点环。
- 至少 40px 的主要触控目标；紧凑阶段导航除外，其高度为 34px。
- `reduceMotion` 为真时禁用共享过渡。

## 数据兼容

兼容入口：

- 旧 `notes.db.txt`。
- 旧 SQLite schema 1。
- JSON v1。
- 配置 schema 1 与旧明文 API Key。
- 工作区 `.chrononotes` v2。

新字段必须为旧数据提供迁移值，例如旧便签没有 `series_id` 时生成稳定标识；
不得通过空指针、隐式窄化或静默截断“凑兼容”。

## 验收入口

Debug：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 `
  -Configuration Debug -BuildDir cmake-build-debug -RunTests
```

干净 Release：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 `
  -Configuration Release -BuildDir cmake-build-release -Clean -RunTests
```

静态检查：

```powershell
cmake --build cmake-build-release --target all_qmllint -j 6
git diff --check
```

CTest 必须包含：

- `note_store_tests`
- `notebook_tests`
- `note_view_tests`
- `project_tree_model_tests`
- `config_tests`
- `ai_client_tests`
- `backup_service_tests`
- `workspace_recovery_tests`
- `qt_note_app_tests`
- `qml_interaction_tests`

关键性能阈值：

- 冷启动 p95 不超过 1.5 秒。
- 空闲 RSS p95 不超过 120 MiB。
- 10,000 条便签查询 p95 不超过 100ms。
- 5,000 节点项目树操作 p95 不超过 100ms。
- 单次持久化 p95 不超过 50ms。

## 提交前检查

- 代码、测试、文档、QML、SVG 和工程脚本可以提交。
- 构建目录、`dist/`、运行数据库、日志、API Key、证书和个人配置不得提交。
- `git diff --cached --check` 必须通过。
- 暂存内容必须扫描常见密钥格式与私钥头。
- 不应提交仅用于截图回归的本地 PNG。

发布工程存在于 `tools/` 与 `.github/workflows/`，但提交源码不要求创建
GitHub Release、Tag、安装包或 PR。
