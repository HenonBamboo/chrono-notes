#include "backup_service.h"

#include <QDateTime>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>

#include <cmath>
#include <limits>
#include <utility>

namespace {

constexpr qint64 kMaximumBackupBytes = 50LL * 1024LL * 1024LL;
constexpr int kMaximumBackupNotes = 100000;
constexpr int kMaximumProjectNodes = 100000;
constexpr int kMaximumSummaries = 10000;
constexpr int kMaximumAppVersionLength = 64;
constexpr int kMaximumProjectTitleLength = 512;
constexpr int kMaximumProjectDescriptionLength = 65536;

void clearError(QString *error) {
    if (error != nullptr) {
        error->clear();
    }
}

bool fail(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

QString fromWide(const wchar_t *value) {
    return value == nullptr ? QString() : QString::fromWCharArray(value);
}

bool fitsWideBuffer(const QString &value, qsizetype capacity, bool allowEmpty) {
    if (value.contains(QChar::Null)) {
        return false;
    }
    const std::wstring wide = value.toStdWString();
    return wide.size() < static_cast<size_t>(capacity) && (allowEmpty || !wide.empty());
}

bool copyWideBuffer(const QString &value, wchar_t *destination, qsizetype capacity,
                    bool allowEmpty) {
    if (destination == nullptr || !fitsWideBuffer(value, capacity, allowEmpty)) {
        return false;
    }
    const std::wstring wide = value.toStdWString();
    std::wmemcpy(destination, wide.c_str(), wide.size() + 1);
    return true;
}

bool jsonInteger(const QJsonValue &value, int *result) {
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < std::numeric_limits<int>::min()
        || number > std::numeric_limits<int>::max()) {
        return false;
    }
    if (result != nullptr) {
        *result = static_cast<int>(number);
    }
    return true;
}

bool jsonInt64(const QJsonValue &value, qint64 *result) {
    bool ok = false;
    qint64 number = 0;
    if (value.isString()) {
        number = value.toString().toLongLong(&ok);
    } else if (value.isDouble()) {
        const double raw = value.toDouble();
        ok = std::isfinite(raw) && std::floor(raw) == raw && raw >= 0.0
            && raw <= static_cast<double>(std::numeric_limits<qint64>::max());
        if (ok) {
            number = static_cast<qint64>(raw);
        }
    }
    if (!ok || number < 0) {
        return false;
    }
    if (result != nullptr) {
        *result = number;
    }
    return true;
}

bool validRepeat(const QString &repeat) {
    return repeat.isEmpty() || repeat == QStringLiteral("daily")
        || repeat == QStringLiteral("weekly") || repeat == QStringLiteral("monthly")
        || repeat == QStringLiteral("yearly");
}

bool validateWorkspaceNote(const BackupService::WorkspaceNote &note, bool requireSeries,
                           QString *error) {
    if (note.id <= 0 || note.id == std::numeric_limits<int>::max()) {
        return fail(error, QStringLiteral("便签 ID 无效。"));
    }
    if (note.stage < NOTE_STAGE_DAY || note.stage > NOTE_STAGE_YEAR) {
        return fail(error, QStringLiteral("便签 stage 无效。"));
    }
    if (!fitsWideBuffer(note.dateKey, NOTE_KEY_MAX, false)) {
        return fail(error, QStringLiteral("便签 dateKey 为空或过长。"));
    }
    if (!fitsWideBuffer(note.text, NOTE_TEXT_MAX, false)) {
        return fail(error, QStringLiteral("便签正文为空或超过 65,536 字符。"));
    }
    if (!fitsWideBuffer(note.repeat, NOTE_REPEAT_MAX, true) || !validRepeat(note.repeat)) {
        return fail(error, QStringLiteral("便签 repeat 无效。"));
    }
    if ((requireSeries || !note.seriesId.isEmpty())
        && !fitsWideBuffer(note.seriesId, NOTE_SERIES_ID_MAX, false)) {
        return fail(error, QStringLiteral("便签 seriesId 为空或过长。"));
    }
    if (note.completedAt < 0 || note.createdAt < 0 || note.updatedAt < 0) {
        return fail(error, QStringLiteral("便签时间字段无效。"));
    }
    return true;
}

bool notebookNoteFromWorkspace(const BackupService::WorkspaceNote &source,
                               bool requireSeries, NotebookNote *note, QString *error);

bool parseWorkspaceNote(const QJsonValue &value, bool requireSeries,
                        BackupService::WorkspaceNote *note, QString *error) {
    if (!value.isObject()) {
        return fail(error, QStringLiteral("便签记录必须是 JSON 对象。"));
    }
    const QJsonObject object = value.toObject();
    BackupService::WorkspaceNote parsed;
    if (!jsonInteger(object.value(QStringLiteral("id")), &parsed.id)
        || !jsonInteger(object.value(QStringLiteral("stage")), &parsed.stage)) {
        return fail(error, QStringLiteral("便签 id 或 stage 字段无效。"));
    }
    if (!object.value(QStringLiteral("dateKey")).isString()
        || !object.value(QStringLiteral("text")).isString()
        || !object.value(QStringLiteral("completed")).isBool()) {
        return fail(error, QStringLiteral("便签必填字段缺失或类型错误。"));
    }
    parsed.dateKey = object.value(QStringLiteral("dateKey")).toString();
    parsed.text = object.value(QStringLiteral("text")).toString();
    parsed.completed = object.value(QStringLiteral("completed")).toBool();
    if (!jsonInt64(object.value(QStringLiteral("completedAt")), &parsed.completedAt)
        || !jsonInt64(object.value(QStringLiteral("createdAt")), &parsed.createdAt)
        || !jsonInt64(object.value(QStringLiteral("updatedAt")), &parsed.updatedAt)) {
        return fail(error, QStringLiteral("便签时间字段缺失或类型错误。"));
    }
    const QJsonValue repeat = object.value(QStringLiteral("repeat"));
    if (!repeat.isUndefined() && !repeat.isString()) {
        return fail(error, QStringLiteral("便签 repeat 必须是字符串。"));
    }
    parsed.repeat = repeat.toString();
    const QJsonValue series = object.value(QStringLiteral("seriesId"));
    if (!series.isUndefined() && !series.isString()) {
        return fail(error, QStringLiteral("便签 seriesId 必须是字符串。"));
    }
    parsed.seriesId = series.toString();
    NotebookNote validated;
    if (!notebookNoteFromWorkspace(parsed, requireSeries, &validated, error)) {
        return false;
    }
    if (note != nullptr) {
        *note = parsed;
    }
    return true;
}

bool parseNotes(const QJsonArray &array, bool requireSeries,
                QList<BackupService::WorkspaceNote> *notes, QString *error) {
    if (array.size() > kMaximumBackupNotes) {
        return fail(error, QStringLiteral("便签数量超过安全上限。"));
    }
    QList<BackupService::WorkspaceNote> parsed;
    parsed.reserve(array.size());
    QSet<int> ids;
    for (const QJsonValue &value : array) {
        BackupService::WorkspaceNote note;
        if (!parseWorkspaceNote(value, requireSeries, &note, error)) {
            return false;
        }
        if (ids.contains(note.id)) {
            return fail(error, QStringLiteral("备份包含重复便签 ID：%1。").arg(note.id));
        }
        ids.insert(note.id);
        parsed.append(note);
    }
    if (notes != nullptr) {
        *notes = parsed;
    }
    return true;
}

QJsonObject noteToJson(const BackupService::WorkspaceNote &note) {
    QJsonObject item;
    item.insert(QStringLiteral("id"), note.id);
    item.insert(QStringLiteral("stage"), note.stage);
    item.insert(QStringLiteral("dateKey"), note.dateKey);
    item.insert(QStringLiteral("text"), note.text);
    item.insert(QStringLiteral("completed"), note.completed);
    item.insert(QStringLiteral("completedAt"), QString::number(note.completedAt));
    item.insert(QStringLiteral("createdAt"), QString::number(note.createdAt));
    item.insert(QStringLiteral("updatedAt"), QString::number(note.updatedAt));
    item.insert(QStringLiteral("repeat"), note.repeat);
    if (!note.seriesId.isEmpty()) {
        item.insert(QStringLiteral("seriesId"), note.seriesId);
    }
    return item;
}

QString repeatName(NotebookRepeat repeat) {
    switch (repeat) {
        case NotebookRepeat::None:
            return {};
        case NotebookRepeat::Daily:
            return QStringLiteral("daily");
        case NotebookRepeat::Weekly:
            return QStringLiteral("weekly");
        case NotebookRepeat::Monthly:
            return QStringLiteral("monthly");
        case NotebookRepeat::Yearly:
            return QStringLiteral("yearly");
    }
    return {};
}

bool validNotebookRepeat(NotebookRepeat repeat) {
    switch (repeat) {
        case NotebookRepeat::None:
        case NotebookRepeat::Daily:
        case NotebookRepeat::Weekly:
        case NotebookRepeat::Monthly:
        case NotebookRepeat::Yearly:
            return true;
    }
    return false;
}

bool repeatFromName(const QString &name, NotebookRepeat *repeat) {
    NotebookRepeat parsed = NotebookRepeat::None;
    if (name.isEmpty()) {
        parsed = NotebookRepeat::None;
    } else if (name == QStringLiteral("daily")) {
        parsed = NotebookRepeat::Daily;
    } else if (name == QStringLiteral("weekly")) {
        parsed = NotebookRepeat::Weekly;
    } else if (name == QStringLiteral("monthly")) {
        parsed = NotebookRepeat::Monthly;
    } else if (name == QStringLiteral("yearly")) {
        parsed = NotebookRepeat::Yearly;
    } else {
        return false;
    }
    if (repeat != nullptr) {
        *repeat = parsed;
    }
    return true;
}

bool notebookStageFromInteger(int stage, NotebookStage *result) {
    NotebookStage parsed = NotebookStage::Day;
    switch (stage) {
        case 0:
            parsed = NotebookStage::Day;
            break;
        case 1:
            parsed = NotebookStage::Week;
            break;
        case 2:
            parsed = NotebookStage::Month;
            break;
        case 3:
            parsed = NotebookStage::Year;
            break;
        default:
            return false;
    }
    if (result != nullptr) {
        *result = parsed;
    }
    return true;
}

bool validNotebookDateKey(NotebookStage stage, const QString &dateKey) {
    if (dateKey.isEmpty() || dateKey.size() >= NOTE_KEY_MAX
        || dateKey.contains(QChar::Null)) {
        return false;
    }
    switch (stage) {
        case NotebookStage::Day: {
            const QDate date = QDate::fromString(dateKey, QStringLiteral("yyyy-MM-dd"));
            return date.isValid() && date.toString(QStringLiteral("yyyy-MM-dd")) == dateKey;
        }
        case NotebookStage::Week: {
            static const QRegularExpression pattern(
                QStringLiteral(R"(^([0-9]{4})-W([0-9]{2})$)"));
            const QRegularExpressionMatch match = pattern.match(dateKey);
            if (!match.hasMatch()) {
                return false;
            }
            const int year = match.captured(1).toInt();
            const int week = match.captured(2).toInt();
            if (year < 1 || week < 1 || week > 53) {
                return false;
            }
            const QDate fourthOfJanuary(year, 1, 4);
            int weekYear = 0;
            const QDate requestedMonday =
                fourthOfJanuary.addDays(1 - fourthOfJanuary.dayOfWeek())
                    .addDays((week - 1) * 7);
            return requestedMonday.weekNumber(&weekYear) == week && weekYear == year;
        }
        case NotebookStage::Month: {
            const QDate date =
                QDate::fromString(dateKey + QStringLiteral("-01"),
                                  QStringLiteral("yyyy-MM-dd"));
            return date.isValid() && date.toString(QStringLiteral("yyyy-MM")) == dateKey;
        }
        case NotebookStage::Year: {
            static const QRegularExpression pattern(QStringLiteral(R"(^[0-9]{4}$)"));
            return pattern.match(dateKey).hasMatch()
                && QDate(dateKey.toInt(), 1, 1).isValid();
        }
    }
    return false;
}

BackupService::WorkspaceNote noteFromNotebook(const NotebookNote &source) {
    BackupService::WorkspaceNote note;
    note.id = source.id;
    note.stage = static_cast<int>(source.stage);
    note.dateKey = source.dateKey;
    note.text = source.text;
    note.completed = source.completed;
    note.completedAt = source.completedAt;
    note.createdAt = source.createdAt;
    note.updatedAt = source.updatedAt;
    note.repeat = repeatName(source.repeat);
    note.seriesId = source.seriesId;
    return note;
}

bool notebookNoteFromWorkspace(const BackupService::WorkspaceNote &source,
                               bool requireSeries, NotebookNote *note, QString *error) {
    if (!validateWorkspaceNote(source, requireSeries, error)) {
        return false;
    }
    NotebookNote converted;
    if (!notebookStageFromInteger(source.stage, &converted.stage)
        || !validNotebookDateKey(converted.stage, source.dateKey)) {
        return fail(error, QStringLiteral("便签阶段或日期键不符合 Notebook 约束。"));
    }
    if (!repeatFromName(source.repeat, &converted.repeat)
        || (converted.stage != NotebookStage::Day
            && converted.repeat != NotebookRepeat::None)) {
        return fail(error, QStringLiteral("重复规则无效；重复便签仅支持每日阶段。"));
    }
    if ((!source.completed && source.completedAt != 0)
        || source.updatedAt < source.createdAt) {
        return fail(error, QStringLiteral("便签完成时间或更新时间无效。"));
    }
    converted.id = source.id;
    converted.dateKey = source.dateKey;
    converted.text = source.text;
    converted.completed = source.completed;
    converted.completedAt = source.completedAt;
    converted.createdAt = source.createdAt;
    converted.updatedAt = source.updatedAt;
    converted.seriesId = source.seriesId;
    if (note != nullptr) {
        *note = converted;
    }
    return true;
}

bool workspaceNotesFromNotebook(const NotebookSnapshot &snapshot,
                                QList<BackupService::WorkspaceNote> *notes,
                                QString *error) {
    if (snapshot.schemaVersion != 1
        && snapshot.schemaVersion != Notebook::CurrentSchemaVersion) {
        return fail(error, QStringLiteral("Notebook 快照 schema 版本不受支持。"));
    }
    if (snapshot.notes.size() > kMaximumBackupNotes) {
        return fail(error, QStringLiteral("便签数量超过安全上限。"));
    }
    const bool requireSeries = snapshot.schemaVersion >= Notebook::CurrentSchemaVersion;
    QList<BackupService::WorkspaceNote> converted;
    converted.reserve(snapshot.notes.size());
    QSet<int> ids;
    for (const NotebookNote &source : snapshot.notes) {
        if (!validNotebookRepeat(source.repeat)) {
            return fail(error, QStringLiteral("Notebook 快照包含无效重复规则。"));
        }
        const BackupService::WorkspaceNote note = noteFromNotebook(source);
        NotebookNote validated;
        if (!notebookNoteFromWorkspace(note, requireSeries, &validated, error)) {
            return false;
        }
        if (ids.contains(note.id)) {
            return fail(error, QStringLiteral("Notebook 快照包含重复便签 ID：%1。")
                                   .arg(note.id));
        }
        ids.insert(note.id);
        converted.append(note);
    }
    if (notes != nullptr) {
        *notes = converted;
    }
    return true;
}

BackupService::WorkspaceNote noteFromEvent(const NoteEvent &event) {
    BackupService::WorkspaceNote note;
    note.id = event.id;
    note.stage = event.stage;
    note.dateKey = fromWide(event.date_key);
    note.text = fromWide(event.text);
    note.completed = event.completed != 0;
    note.completedAt = event.completed_at;
    note.createdAt = event.created_at;
    note.updatedAt = event.updated_at;
    note.repeat = fromWide(event.repeat);
    note.seriesId = fromWide(event.series_id);
    return note;
}

bool eventFromNote(const BackupService::WorkspaceNote &note, NoteEvent *event) {
    if (event == nullptr) {
        return false;
    }
    NoteEvent converted{};
    converted.id = note.id;
    converted.stage = static_cast<NoteStage>(note.stage);
    converted.completed = note.completed ? 1 : 0;
    converted.completed_at = note.completedAt;
    converted.created_at = note.createdAt;
    converted.updated_at = note.updatedAt;
    if (!copyWideBuffer(note.dateKey, converted.date_key, NOTE_KEY_MAX, false)
        || !copyWideBuffer(note.text, converted.text, NOTE_TEXT_MAX, false)
        || !copyWideBuffer(note.repeat, converted.repeat, NOTE_REPEAT_MAX, true)
        || (!note.seriesId.isEmpty()
            && !copyWideBuffer(note.seriesId, converted.series_id, NOTE_SERIES_ID_MAX, false))) {
        return false;
    }
    *event = converted;
    return true;
}

bool readFile(const QUrl &fileUrl, QByteArray *bytes, QString *path, QString *error) {
    const QString resolvedPath = BackupService::localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        return fail(error, QStringLiteral("请选择要导入的本地备份文件。"));
    }
    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error, QStringLiteral("无法读取备份文件：%1").arg(file.errorString()));
    }
    if (file.size() < 0 || file.size() > kMaximumBackupBytes) {
        return fail(error, QStringLiteral("备份文件超过 50 MiB 安全上限。"));
    }
    const QByteArray content = file.readAll();
    if (file.error() != QFileDevice::NoError || content.size() != file.size()) {
        return fail(error, QStringLiteral("备份文件读取不完整：%1").arg(file.errorString()));
    }
    if (bytes != nullptr) {
        *bytes = content;
    }
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

bool parseV1Document(const QByteArray &bytes, QList<BackupService::WorkspaceNote> *notes,
                     QString *error) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (!document.isObject()) {
        return fail(error, QStringLiteral("JSON 格式无效：%1").arg(parseError.errorString()));
    }
    const QJsonObject root = document.object();
    int version = 0;
    if (!jsonInteger(root.value(QStringLiteral("version")), &version) || version != 1) {
        return fail(error, QStringLiteral("仅支持便签 JSON v1。"));
    }
    if (!root.value(QStringLiteral("events")).isArray()) {
        return fail(error, QStringLiteral("便签 JSON 缺少 events 数组。"));
    }
    return parseNotes(root.value(QStringLiteral("events")).toArray(), false, notes, error);
}

bool ensureParentDirectory(const QString &path, QString *error) {
    const QString parentPath = QFileInfo(path).absolutePath();
    if (QDir(parentPath).exists() || QDir().mkpath(parentPath)) {
        return true;
    }
    return fail(error, QStringLiteral("无法创建导出目录。"));
}

bool writeAtomic(const QString &path, const QByteArray &bytes, const QString &label,
                 QString *error) {
    if (!ensureParentDirectory(path, error)) {
        return false;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(error, QStringLiteral("无法写入%1：%2").arg(label, file.errorString()));
    }
    if (file.write(bytes) != bytes.size()) {
        const QString message = QStringLiteral("%1写入不完整：%2").arg(label, file.errorString());
        file.cancelWriting();
        return fail(error, message);
    }
    if (!file.commit()) {
        return fail(error, QStringLiteral("无法原子提交%1：%2").arg(label, file.errorString()));
    }
    return true;
}

bool exportJsonNotes(const QList<BackupService::WorkspaceNote> &notes,
                     bool requireSeries, const QUrl &fileUrl,
                     QString *path, QString *error) {
    const QString resolvedPath = BackupService::localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        return fail(error, QStringLiteral("请选择 JSON 导出位置。"));
    }
    QJsonArray events;
    QSet<int> ids;
    for (const BackupService::WorkspaceNote &note : notes) {
        NotebookNote validated;
        if (!notebookNoteFromWorkspace(note, requireSeries, &validated, error)) {
            return false;
        }
        if (ids.contains(note.id)) {
            return fail(error, QStringLiteral("便签快照包含重复 ID：%1。").arg(note.id));
        }
        ids.insert(note.id);
        events.append(noteToJson(note));
    }
    if (events.size() > kMaximumBackupNotes) {
        return fail(error, QStringLiteral("便签数量超过安全上限。"));
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("events"), events);
    if (!writeAtomic(resolvedPath, QJsonDocument(root).toJson(QJsonDocument::Indented),
                     QStringLiteral("JSON 备份"), error)) {
        return false;
    }
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

QString stageLabel(int stage) {
    switch (stage) {
        case 0:
            return QStringLiteral("每天");
        case 1:
            return QStringLiteral("每周");
        case 2:
            return QStringLiteral("每月");
        case 3:
            return QStringLiteral("每年");
        default:
            return QStringLiteral("未知");
    }
}

bool exportMarkdownNotes(const QList<BackupService::WorkspaceNote> &notes,
                         bool requireSeries, const QUrl &fileUrl,
                         QString *path, QString *error) {
    const QString resolvedPath = BackupService::localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        return fail(error, QStringLiteral("请选择 Markdown 导出位置。"));
    }
    QSet<int> ids;
    for (const BackupService::WorkspaceNote &note : notes) {
        NotebookNote validated;
        if (!notebookNoteFromWorkspace(note, requireSeries, &validated, error)) {
            return false;
        }
        if (ids.contains(note.id)) {
            return fail(error, QStringLiteral("便签快照包含重复 ID：%1。").arg(note.id));
        }
        ids.insert(note.id);
    }
    if (notes.size() > kMaximumBackupNotes) {
        return fail(error, QStringLiteral("便签数量超过安全上限。"));
    }

    QString markdown;
    QTextStream out(&markdown);
    out << "# ChronoNotes 便签导出\n\n";
    for (int stage = 0; stage <= 3; ++stage) {
        out << "## " << stageLabel(stage) << "\n\n";
        for (const BackupService::WorkspaceNote &note : notes) {
            if (note.stage != stage) {
                continue;
            }
            out << "- [" << (note.completed ? "x" : " ") << "] "
                << note.dateKey << " " << note.text;
            if (!note.repeat.isEmpty()) {
                out << " (repeat: " << note.repeat << ")";
            }
            out << "\n";
        }
        out << "\n";
    }
    if (!writeAtomic(resolvedPath, markdown.toUtf8(), QStringLiteral("Markdown 备份"), error)) {
        return false;
    }
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

bool validateProjectSnapshot(const QJsonObject &projects, QString *error) {
    int version = 0;
    if (!jsonInteger(projects.value(QStringLiteral("version")), &version) || version != 1
        || !projects.value(QStringLiteral("nodes")).isArray()) {
        return fail(error, QStringLiteral("项目树快照必须是有效的 v1 文档。"));
    }
    const QJsonArray nodes = projects.value(QStringLiteral("nodes")).toArray();
    if (nodes.size() > kMaximumProjectNodes) {
        return fail(error, QStringLiteral("项目树节点数量超过安全上限。"));
    }
    QSet<int> ids;
    QHash<int, int> parents;
    int maximumId = 0;
    for (const QJsonValue &value : nodes) {
        if (!value.isObject()) {
            return fail(error, QStringLiteral("项目树节点必须是 JSON 对象。"));
        }
        const QJsonObject node = value.toObject();
        int id = 0;
        int parentId = 0;
        if (!jsonInteger(node.value(QStringLiteral("id")), &id) || id <= 0
            || !jsonInteger(node.value(QStringLiteral("parentId")), &parentId) || parentId < 0
            || ids.contains(id)) {
            return fail(error, QStringLiteral("项目树节点 ID 无效或重复。"));
        }
        if (!node.value(QStringLiteral("title")).isString()
            || node.value(QStringLiteral("title")).toString().trimmed().isEmpty()
            || node.value(QStringLiteral("title")).toString().trimmed().size()
                > kMaximumProjectTitleLength
            || !node.value(QStringLiteral("kind")).isString()) {
            return fail(error, QStringLiteral("项目树节点标题或 kind 无效。"));
        }
        const QString kind = node.value(QStringLiteral("kind")).toString();
        if ((kind != QStringLiteral("project") && kind != QStringLiteral("task"))
            || ((parentId == 0) != (kind == QStringLiteral("project")))) {
            return fail(error, QStringLiteral("项目树节点 kind 与层级不一致。"));
        }
        const QJsonValue description = node.value(QStringLiteral("description"));
        if ((!description.isUndefined() && !description.isString())
            || description.toString().size() > kMaximumProjectDescriptionLength) {
            return fail(error, QStringLiteral("项目树节点 description 无效。"));
        }
        if (!node.value(QStringLiteral("completed")).isBool()
            || !node.value(QStringLiteral("expanded")).isBool()) {
            return fail(error, QStringLiteral("项目树节点状态字段无效。"));
        }
        qint64 timestamp = 0;
        if (!jsonInt64(node.value(QStringLiteral("createdAt")), &timestamp)
            || !jsonInt64(node.value(QStringLiteral("updatedAt")), &timestamp)) {
            return fail(error, QStringLiteral("项目树节点时间字段无效。"));
        }
        ids.insert(id);
        parents.insert(id, parentId);
        maximumId = std::max(maximumId, id);
    }
    for (auto it = parents.cbegin(); it != parents.cend(); ++it) {
        if (it.value() != 0 && !ids.contains(it.value())) {
            return fail(error, QStringLiteral("项目树包含孤立节点。"));
        }
    }
    QHash<int, int> state;
    for (const int id : std::as_const(ids)) {
        int current = id;
        QVector<int> chain;
        while (current != 0 && state.value(current, 0) != 2) {
            if (state.value(current, 0) == 1) {
                return fail(error, QStringLiteral("项目树包含循环引用。"));
            }
            state.insert(current, 1);
            chain.append(current);
            current = parents.value(current, 0);
        }
        for (const int visited : std::as_const(chain)) {
            state.insert(visited, 2);
        }
    }
    const QJsonValue nextIdValue = projects.value(QStringLiteral("nextId"));
    if (!nextIdValue.isUndefined()) {
        int nextId = 0;
        if (!jsonInteger(nextIdValue, &nextId) || nextId <= maximumId) {
            return fail(error, QStringLiteral("项目树 nextId 无效。"));
        }
    }
    return true;
}

QString normalizedKey(const QString &key) {
    QString normalized;
    for (const QChar ch : key) {
        if (ch.isLetterOrNumber()) {
            normalized.append(ch.toLower());
        }
    }
    return normalized;
}

bool isSensitivePreferenceKey(const QString &key) {
    const QString normalized = normalizedKey(key);
    return normalized == QStringLiteral("key") || normalized.contains(QStringLiteral("apikey"))
        || normalized.contains(QStringLiteral("credential"))
        || normalized.contains(QStringLiteral("password"))
        || normalized.contains(QStringLiteral("secret"))
        || normalized.contains(QStringLiteral("token"))
        || normalized.contains(QStringLiteral("diagnosticlog"))
        || normalized.contains(QStringLiteral("operationlog"));
}

QJsonValue sanitizePreferences(const QJsonValue &value) {
    if (value.isObject()) {
        QJsonObject sanitized;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!isSensitivePreferenceKey(it.key())) {
                sanitized.insert(it.key(), sanitizePreferences(it.value()));
            }
        }
        return sanitized;
    }
    if (value.isArray()) {
        QJsonArray sanitized;
        for (const QJsonValue &item : value.toArray()) {
            sanitized.append(sanitizePreferences(item));
        }
        return sanitized;
    }
    return value;
}

bool containsSensitivePreference(const QJsonValue &value) {
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (isSensitivePreferenceKey(it.key()) || containsSensitivePreference(it.value())) {
                return true;
            }
        }
    } else if (value.isArray()) {
        for (const QJsonValue &item : value.toArray()) {
            if (containsSensitivePreference(item)) {
                return true;
            }
        }
    }
    return false;
}

bool validateSummaries(const QJsonArray &summaries, QString *error) {
    if (summaries.size() > kMaximumSummaries) {
        return fail(error, QStringLiteral("摘要历史数量超过安全上限。"));
    }
    for (const QJsonValue &summary : summaries) {
        if (!summary.isObject()) {
            return fail(error, QStringLiteral("摘要历史记录必须是 JSON 对象。"));
        }
    }
    return true;
}

}  // namespace

namespace BackupService {

QString localPathFromUrl(const QUrl &url) {
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    if (url.scheme().isEmpty()) {
        return url.toString(QUrl::PreferLocalFile);
    }
    return {};
}

int previewJsonEventCount(const QUrl &fileUrl, QString *error) {
    clearError(error);
    QByteArray bytes;
    if (!readFile(fileUrl, &bytes, nullptr, error)) {
        return -1;
    }
    QList<WorkspaceNote> notes;
    if (!parseV1Document(bytes, &notes, error)) {
        return -1;
    }
    return notes.size();
}

bool exportJson(const NoteStore &store, const QUrl &fileUrl, QString *path, QString *error) {
    clearError(error);
    const QString resolvedPath = localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        return fail(error, QStringLiteral("请选择 JSON 导出位置。"));
    }

    QJsonArray events;
    QSet<int> ids;
    for (const NoteEvent &event : store.items) {
        const WorkspaceNote note = noteFromEvent(event);
        NotebookNote validated;
        if (!notebookNoteFromWorkspace(note, true, &validated, error)
            || ids.contains(note.id)) {
            if (error != nullptr && error->isEmpty()) {
                *error = QStringLiteral("便签仓库包含重复 ID。");
            }
            return false;
        }
        ids.insert(note.id);
        events.append(noteToJson(note));
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("events"), events);
    if (!writeAtomic(resolvedPath, QJsonDocument(root).toJson(QJsonDocument::Indented),
                     QStringLiteral("JSON 备份"), error)) {
        return false;
    }
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

bool importJson(const QUrl &fileUrl, NoteStore *store, QString *path, QString *error) {
    clearError(error);
    if (store == nullptr) {
        return fail(error, QStringLiteral("导入目标不可用。"));
    }
    QByteArray bytes;
    QString resolvedPath;
    if (!readFile(fileUrl, &bytes, &resolvedPath, error)) {
        return false;
    }
    QList<WorkspaceNote> notes;
    if (!parseV1Document(bytes, &notes, error)) {
        return false;
    }

    NoteStore imported{};
    note_store_init(&imported);
    for (const WorkspaceNote &note : std::as_const(notes)) {
        NoteEvent event{};
        if (!eventFromNote(note, &event) || !note_store_restore(&imported, &event)) {
            const QString storeError = fromWide(note_store_last_error());
            return fail(error, storeError.isEmpty()
                                  ? QStringLiteral("无法提交导入便签。")
                                  : QStringLiteral("无法提交导入便签：%1").arg(storeError));
        }
    }
    *store = std::move(imported);
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

bool exportMarkdown(const NoteStore &store, const QUrl &fileUrl, QString *path, QString *error) {
    clearError(error);
    const QString resolvedPath = localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        return fail(error, QStringLiteral("请选择 Markdown 导出位置。"));
    }

    QString markdown;
    QTextStream out(&markdown);
    out << "# ChronoNotes 便签导出\n\n";
    for (int stage = NOTE_STAGE_DAY; stage <= NOTE_STAGE_YEAR; ++stage) {
        out << "## " << fromWide(note_stage_label(static_cast<NoteStage>(stage))) << "\n\n";
        for (const NoteEvent &event : store.items) {
            const WorkspaceNote note = noteFromEvent(event);
            NotebookNote validated;
            if (!notebookNoteFromWorkspace(note, true, &validated, error)) {
                return false;
            }
            if (note.stage != stage) {
                continue;
            }
            out << "- [" << (note.completed ? "x" : " ") << "] "
                << note.dateKey << " " << note.text;
            if (!note.repeat.isEmpty()) {
                out << " (repeat: " << note.repeat << ")";
            }
            out << "\n";
        }
        out << "\n";
    }
    if (!writeAtomic(resolvedPath, markdown.toUtf8(), QStringLiteral("Markdown 备份"), error)) {
        return false;
    }
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

bool exportJson(const NotebookSnapshot &snapshot, const QUrl &fileUrl,
                QString *path, QString *error) {
    clearError(error);
    QList<WorkspaceNote> notes;
    if (!workspaceNotesFromNotebook(snapshot, &notes, error)) {
        return false;
    }
    return exportJsonNotes(notes,
                           snapshot.schemaVersion >= Notebook::CurrentSchemaVersion,
                           fileUrl, path, error);
}

bool importJson(const QUrl &fileUrl, NotebookSnapshot *snapshot,
                QString *path, QString *error) {
    clearError(error);
    if (snapshot == nullptr) {
        return fail(error, QStringLiteral("Notebook 导入目标不可用。"));
    }
    QByteArray bytes;
    QString resolvedPath;
    if (!readFile(fileUrl, &bytes, &resolvedPath, error)) {
        return false;
    }
    QList<WorkspaceNote> notes;
    if (!parseV1Document(bytes, &notes, error)) {
        return false;
    }

    NotebookSnapshot imported;
    imported.schemaVersion = 1;
    imported.notes.reserve(notes.size());
    for (const WorkspaceNote &source : std::as_const(notes)) {
        NotebookNote note;
        if (!notebookNoteFromWorkspace(source, false, &note, error)) {
            return false;
        }
        imported.notes.append(note);
    }
    *snapshot = imported;
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

bool importJson(const QUrl &, std::nullptr_t, QString *, QString *error) {
    clearError(error);
    return fail(error, QStringLiteral("导入目标不可用。"));
}

bool exportMarkdown(const NotebookSnapshot &snapshot, const QUrl &fileUrl,
                    QString *path, QString *error) {
    clearError(error);
    QList<WorkspaceNote> notes;
    if (!workspaceNotesFromNotebook(snapshot, &notes, error)) {
        return false;
    }
    return exportMarkdownNotes(notes,
                               snapshot.schemaVersion >= Notebook::CurrentSchemaVersion,
                               fileUrl, path, error);
}

QByteArray encodeWorkspaceV2(const WorkspaceSnapshot &snapshot, QString *error) {
    clearError(error);
    const QString createdAt = snapshot.createdAt.isEmpty()
        ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
        : snapshot.createdAt;
    if (!QDateTime::fromString(createdAt, Qt::ISODate).isValid()) {
        fail(error, QStringLiteral("工作区备份 createdAt 无效。"));
        return {};
    }
    if (snapshot.appVersion.trimmed().isEmpty()
        || snapshot.appVersion.size() > kMaximumAppVersionLength) {
        fail(error, QStringLiteral("工作区备份 appVersion 无效。"));
        return {};
    }

    QJsonArray notes;
    QSet<int> ids;
    for (const WorkspaceNote &note : snapshot.notes) {
        NotebookNote validated;
        if (!notebookNoteFromWorkspace(note, true, &validated, error)) {
            return {};
        }
        if (ids.contains(note.id)) {
            fail(error, QStringLiteral("工作区备份包含重复便签 ID：%1。").arg(note.id));
            return {};
        }
        ids.insert(note.id);
        notes.append(noteToJson(note));
    }
    if (notes.size() > kMaximumBackupNotes) {
        fail(error, QStringLiteral("便签数量超过安全上限。"));
        return {};
    }

    QJsonObject projects = snapshot.projects;
    if (projects.isEmpty()) {
        projects.insert(QStringLiteral("version"), 1);
        projects.insert(QStringLiteral("nextId"), 1);
        projects.insert(QStringLiteral("nodes"), QJsonArray{});
    }
    if (!validateProjectSnapshot(projects, error) || !validateSummaries(snapshot.summaries, error)) {
        return {};
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("chrononotes-workspace"));
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("createdAt"), createdAt);
    root.insert(QStringLiteral("appVersion"), snapshot.appVersion);
    root.insert(QStringLiteral("notes"), notes);
    root.insert(QStringLiteral("projects"), projects);
    root.insert(QStringLiteral("summaries"), snapshot.summaries);
    root.insert(QStringLiteral("preferences"), sanitizePreferences(snapshot.preferences));
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (bytes.size() > kMaximumBackupBytes) {
        fail(error, QStringLiteral("工作区备份超过 50 MiB 安全上限。"));
        return {};
    }
    return bytes;
}

bool decodeWorkspaceV2(const QByteArray &bytes, WorkspaceSnapshot *snapshot, QString *error) {
    clearError(error);
    if (snapshot == nullptr) {
        return fail(error, QStringLiteral("工作区导入目标不可用。"));
    }
    if (bytes.size() > kMaximumBackupBytes) {
        return fail(error, QStringLiteral("工作区备份超过 50 MiB 安全上限。"));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (!document.isObject()) {
        return fail(error, QStringLiteral("工作区 JSON 无效：%1").arg(parseError.errorString()));
    }
    const QJsonObject root = document.object();
    int version = 0;
    if (root.value(QStringLiteral("format")).toString()
            != QStringLiteral("chrononotes-workspace")
        || !jsonInteger(root.value(QStringLiteral("version")), &version) || version != 2) {
        return fail(error, QStringLiteral("仅支持 ChronoNotes 工作区备份 v2。"));
    }
    if (!root.value(QStringLiteral("createdAt")).isString()
        || !QDateTime::fromString(root.value(QStringLiteral("createdAt")).toString(),
                                  Qt::ISODate).isValid()
        || !root.value(QStringLiteral("appVersion")).isString()
        || root.value(QStringLiteral("appVersion")).toString().trimmed().isEmpty()
        || root.value(QStringLiteral("appVersion")).toString().size() > kMaximumAppVersionLength
        || !root.value(QStringLiteral("notes")).isArray()
        || !root.value(QStringLiteral("projects")).isObject()
        || !root.value(QStringLiteral("summaries")).isArray()
        || !root.value(QStringLiteral("preferences")).isObject()) {
        return fail(error, QStringLiteral("工作区备份字段缺失或类型错误。"));
    }

    WorkspaceSnapshot parsed;
    parsed.createdAt = root.value(QStringLiteral("createdAt")).toString();
    parsed.appVersion = root.value(QStringLiteral("appVersion")).toString();
    if (!parseNotes(root.value(QStringLiteral("notes")).toArray(), true, &parsed.notes, error)) {
        return false;
    }
    parsed.projects = root.value(QStringLiteral("projects")).toObject();
    if (!validateProjectSnapshot(parsed.projects, error)) {
        return false;
    }
    parsed.summaries = root.value(QStringLiteral("summaries")).toArray();
    if (!validateSummaries(parsed.summaries, error)) {
        return false;
    }
    parsed.preferences = root.value(QStringLiteral("preferences")).toObject();
    if (containsSensitivePreference(parsed.preferences)) {
        return fail(error, QStringLiteral("工作区偏好包含凭据或日志字段。"));
    }
    *snapshot = parsed;
    return true;
}

bool exportWorkspaceV2(const WorkspaceSnapshot &snapshot, const QUrl &fileUrl,
                       QString *path, QString *error) {
    clearError(error);
    const QString resolvedPath = localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        return fail(error, QStringLiteral("请选择工作区导出位置。"));
    }
    const QByteArray bytes = encodeWorkspaceV2(snapshot, error);
    if (bytes.isEmpty() || !writeAtomic(resolvedPath, bytes, QStringLiteral("工作区备份"), error)) {
        return false;
    }
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

bool importWorkspaceV2(const QUrl &fileUrl, WorkspaceSnapshot *snapshot,
                       QString *path, QString *error) {
    clearError(error);
    QByteArray bytes;
    QString resolvedPath;
    if (!readFile(fileUrl, &bytes, &resolvedPath, error)
        || !decodeWorkspaceV2(bytes, snapshot, error)) {
        return false;
    }
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

}  // namespace BackupService
