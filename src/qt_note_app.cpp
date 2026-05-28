#include "qt_note_app.h"

#include "ai_client.h"
#include "backup_service.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimeZone>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>
#include <cstring>
#include <ctime>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

NoteApp::NoteApp(QObject *parent) : QAbstractListModel(parent) {
    initPaths();
    QDir().mkpath(fromWide(data_dir_));
    note_store_init(&store_);
    if (!QFileInfo::exists(fromWide(sqlite_path_)) && QFileInfo::exists(fromWide(notes_path_))) {
        note_store_load(&store_, notes_path_);
        note_store_save_sqlite(&store_, sqlite_path_);
    } else {
        note_store_load_sqlite(&store_, sqlite_path_);
    }
    config_defaults(&config_);
    if (!config_load(&config_, config_path_)) {
        config_save(&config_, config_path_);
    } else if (config_.api_key[0] == L'\0' &&
               wcscmp(config_.api_url, L"https://api.openai.com/v1/chat/completions") == 0 &&
               wcscmp(config_.model, L"gpt-4o-mini") == 0) {
        config_defaults(&config_);
        config_save(&config_, config_path_);
    }
    updateDateKey();
    reload();
}

int NoteApp::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : rows_.size();
}

QVariant NoteApp::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }
    const RowItem &row = rows_.at(index.row());
    const NoteEvent *event = row.event;
    if (event == nullptr) {
        return {};
    }
    switch (role) {
        case IdRole: return event->id;
        case TextRole: return fromWide(event->text);
        case CompletedRole: return event->completed != 0;
        case MetaRole: return row.meta;
        case SectionRole: return row.section;
        case SectionFirstRole: return row.section_first;
        case ReadOnlyRole: return row.archive;
        case ArchiveRole: return row.archive;
        case HighlightedTextRole: return row.highlighted_text;
        case RepeatRole: return fromWide(event->repeat);
        default: return {};
    }
}

QHash<int, QByteArray> NoteApp::roleNames() const {
    return {
        {IdRole, "eventId"},
        {TextRole, "text"},
        {CompletedRole, "completed"},
        {MetaRole, "meta"},
        {SectionRole, "section"},
        {SectionFirstRole, "sectionFirst"},
        {ReadOnlyRole, "readOnly"},
        {ArchiveRole, "archive"},
        {HighlightedTextRole, "highlightedText"},
        {RepeatRole, "repeat"}
    };
}

int NoteApp::stage() const {
    return static_cast<int>(stage_);
}

void NoteApp::setStage(int value) {
    if (value < NOTE_STAGE_DAY || value > NOTE_STAGE_YEAR || value == stage_) {
        return;
    }
    stage_ = static_cast<NoteStage>(value);
    updateDateKey();
    reload();
    emit stageChanged();
}

QString NoteApp::stageLabel() const {
    return fromWide(note_stage_label(stage_));
}

QString NoteApp::dateKey() const {
    return fromWide(date_key_);
}

int NoteApp::completedCount() const {
    int done = 0;
    for (const RowItem &row : rows_) {
        if (!row.archive && row.event != nullptr && row.event->completed) ++done;
    }
    return done;
}

int NoteApp::totalCount() const {
    int total = 0;
    for (const RowItem &row : rows_) {
        if (!row.archive) ++total;
    }
    return total;
}

bool NoteApp::allCompleted() const {
    const int total = totalCount();
    return total > 0 && completedCount() == total;
}

bool NoteApp::hasVisibleRows() const {
    return !rows_.isEmpty();
}

bool NoteApp::canUndo() const {
    return undo_type_ != UndoType::None &&
           undo_expires_at_ >= QDateTime::currentMSecsSinceEpoch();
}

int NoteApp::viewRevision() const {
    return view_revision_;
}

QString NoteApp::notice() const {
    return notice_;
}

QString NoteApp::searchQuery() const {
    return search_query_;
}

void NoteApp::setSearchQuery(const QString &value) {
    const QString trimmed = value.trimmed();
    if (search_query_ == trimmed) {
        return;
    }
    search_query_ = trimmed;
    reload();
    emit searchChanged();
}

bool NoteApp::searchActive() const {
    return !search_query_.isEmpty();
}

int NoteApp::searchCompletionFilter() const {
    return search_completion_filter_;
}

void NoteApp::setSearchCompletionFilter(int value) {
    const int normalized = value < 0 ? -1 : (value > 0 ? 1 : 0);
    if (search_completion_filter_ == normalized) {
        return;
    }
    search_completion_filter_ = normalized;
    reload();
    emit searchChanged();
}

bool NoteApp::hasSelectedEvent() const {
    return findEventConst(selected_event_id_) != nullptr;
}

int NoteApp::selectedEventId() const {
    return hasSelectedEvent() ? selected_event_id_ : -1;
}

QString NoteApp::selectedEventText() const {
    const NoteEvent *event = findEventConst(selected_event_id_);
    return event != nullptr ? fromWide(event->text) : QString();
}

QString NoteApp::selectedEventMeta() const {
    const NoteEvent *event = findEventConst(selected_event_id_);
    if (event == nullptr) {
        return {};
    }
    return selected_event_read_only_ ? NoteView::archiveMetaFor(event) : NoteView::metaFor(event);
}

QString NoteApp::selectedEventRepeat() const {
    const NoteEvent *event = findEventConst(selected_event_id_);
    return event != nullptr ? fromWide(event->repeat) : QString();
}

bool NoteApp::selectedEventReadOnly() const {
    return hasSelectedEvent() && selected_event_read_only_;
}

QString NoteApp::apiUrl() const {
    return fromWide(config_.api_url);
}

void NoteApp::setApiUrl(const QString &value) {
    const std::wstring wide = toWide(value);
    wcsncpy(config_.api_url, wide.c_str(), CONFIG_VALUE_MAX - 1);
    config_.api_url[CONFIG_VALUE_MAX - 1] = L'\0';
    emit configChanged();
}

QString NoteApp::apiKey() const {
    return fromWide(config_.api_key);
}

void NoteApp::setApiKey(const QString &value) {
    const std::wstring wide = toWide(value);
    wcsncpy(config_.api_key, wide.c_str(), CONFIG_VALUE_MAX - 1);
    config_.api_key[CONFIG_VALUE_MAX - 1] = L'\0';
    emit configChanged();
}

QString NoteApp::modelName() const {
    return fromWide(config_.model);
}

void NoteApp::setModelName(const QString &value) {
    const std::wstring wide = toWide(value);
    wcsncpy(config_.model, wide.c_str(), 127);
    config_.model[127] = L'\0';
    emit configChanged();
}

void NoteApp::addEvent(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    const std::wstring wide = toWide(trimmed);
    NoteEvent *event = note_store_add(&store_, stage_, date_key_, wide.c_str());
    if (event == nullptr) {
        setNotice(QStringLiteral("事件创建失败，请检查内容后重试。"));
        return;
    }
    saveNotes();
    const QVector<RowItem> next = buildRows();
    int insert_row = -1;
    for (int i = 0; i < next.size(); ++i) {
        if (next.at(i).event != nullptr && next.at(i).event->id == event->id) {
            insert_row = i;
            break;
        }
    }
    if (insert_row >= 0 && next.size() == rows_.size() + 1) {
        beginInsertRows(QModelIndex(), insert_row, insert_row);
        rows_ = next;
        endInsertRows();
        if (!rows_.isEmpty()) {
            emit dataChanged(index(0, 0), index(rows_.size() - 1, 0));
        }
        ++view_revision_;
        emit countsChanged();
        emit viewRevisionChanged();
    } else {
        reload();
    }
    setNotice(QStringLiteral("已添加新事件。"));
    appendOperationLog(QStringLiteral("add"), event);
}

void NoteApp::toggleEvent(int id) {
    NoteEvent *event = findEvent(id);
    if (event == nullptr) {
        return;
    }
    const NoteEvent snapshot = *event;
    if (note_store_toggle(&store_, id)) {
        rememberUndo(UndoType::Toggle, snapshot);
        createNextRepeatIfNeeded(snapshot);
        saveNotes();
        reloadModelChange();
        if (selected_event_id_ == id) {
            emit selectedEventChanged();
        }
        appendOperationLog(QStringLiteral("toggle"), findEventConst(id));
        setNotice(QStringLiteral("已更新完成状态，可按 Ctrl+Z 撤销。"));
    }
}

void NoteApp::deleteEvent(int id) {
    int remove_row = -1;
    NoteEvent snapshot{};
    bool has_snapshot = false;
    for (int i = 0; i < rows_.size(); ++i) {
        if (rows_.at(i).event != nullptr && rows_.at(i).event->id == id) {
            remove_row = i;
            snapshot = *rows_.at(i).event;
            has_snapshot = true;
            break;
        }
    }

    if (remove_row >= 0) {
        beginRemoveRows(QModelIndex(), remove_row, remove_row);
        const int removed = note_store_delete(&store_, id);
        rows_ = buildRows();
        endRemoveRows();
        if (removed) {
            if (has_snapshot) {
                rememberUndo(UndoType::Delete, snapshot);
            }
            saveNotes();
            if (!rows_.isEmpty()) {
                emit dataChanged(index(0, 0), index(rows_.size() - 1, 0));
            }
            ++view_revision_;
            emit countsChanged();
            emit viewRevisionChanged();
            if (selected_event_id_ == id) {
                clearSelectedEvent();
            }
            appendOperationLog(QStringLiteral("delete"), &snapshot);
            setNotice(QStringLiteral("已删除事件，可按 Ctrl+Z 撤销。"));
        }
        return;
    }

    NoteEvent *event = findEvent(id);
    if (event != nullptr) {
        snapshot = *event;
        has_snapshot = true;
    }
    if (note_store_delete(&store_, id)) {
        if (has_snapshot) {
            rememberUndo(UndoType::Delete, snapshot);
        }
        saveNotes();
        reload();
        if (selected_event_id_ == id) {
            clearSelectedEvent();
        }
        appendOperationLog(QStringLiteral("delete"), &snapshot);
        setNotice(QStringLiteral("已删除事件，可按 Ctrl+Z 撤销。"));
    }
}

void NoteApp::updateEvent(int id, const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    const std::wstring wide = toWide(trimmed);
    if (note_store_update_text(&store_, id, wide.c_str())) {
        clearUndo();
        saveNotes();
        reloadModelChange();
        if (selected_event_id_ == id) {
            emit selectedEventChanged();
        }
        appendOperationLog(QStringLiteral("update"), findEventConst(id));
    }
}

void NoteApp::toggleAll() {
    note_store_toggle_all(&store_, stage_, date_key_);
    clearUndo();
    saveNotes();
    reloadModelChange();
    appendOperationLog(QStringLiteral("toggle_all"), nullptr, QStringLiteral("%1 %2").arg(stageLabel(), dateKey()));
}

void NoteApp::clearCompletedCurrent() {
    int removed = 0;
    for (int i = 0; i < store_.count;) {
        NoteEvent *event = &store_.items[i];
        if (event->stage == stage_ && wcscmp(event->date_key, date_key_) == 0 && event->completed) {
            note_store_delete(&store_, event->id);
            ++removed;
        } else {
            ++i;
        }
    }
    if (removed > 0) {
        clearUndo();
        saveNotes();
        reload();
        syncSelectedEvent();
        appendOperationLog(QStringLiteral("clear_completed_current"), nullptr, QString::number(removed));
        setNotice(QStringLiteral("已删除当前阶段的已完成事件。"));
    } else {
        setNotice(QStringLiteral("当前阶段没有已完成事件。"));
    }
}

void NoteApp::clearCurrentStage() {
    int removed = 0;
    for (int i = 0; i < store_.count;) {
        NoteEvent *event = &store_.items[i];
        if (event->stage == stage_ && wcscmp(event->date_key, date_key_) == 0) {
            note_store_delete(&store_, event->id);
            ++removed;
        } else {
            ++i;
        }
    }
    if (removed > 0) {
        clearUndo();
        saveNotes();
        reload();
        syncSelectedEvent();
        appendOperationLog(QStringLiteral("clear_current_stage"), nullptr, QString::number(removed));
        setNotice(QStringLiteral("已清空当前阶段计划。"));
    } else {
        setNotice(QStringLiteral("当前阶段没有可清理的计划。"));
    }
}

void NoteApp::clearAllNotes() {
    note_store_init(&store_);
    clearUndo();
    clearSelectedEvent();
    saveNotes();
    reload();
    appendOperationLog(QStringLiteral("clear_all"));
    setNotice(QStringLiteral("已清空全部便签。"));
}

void NoteApp::clearCompletedAll() {
    int removed = 0;
    for (int i = 0; i < store_.count;) {
        NoteEvent *event = &store_.items[i];
        if (event->completed) {
            note_store_delete(&store_, event->id);
            ++removed;
        } else {
            ++i;
        }
    }
    if (removed > 0) {
        clearUndo();
        saveNotes();
        reload();
        syncSelectedEvent();
        appendOperationLog(QStringLiteral("clear_completed_all"), nullptr, QString::number(removed));
        setNotice(QStringLiteral("已删除全部已完成事件。"));
    } else {
        setNotice(QStringLiteral("没有已完成事件可清理。"));
    }
}

void NoteApp::undoLastAction() {
    if (!canUndo()) {
        clearUndo();
        setNotice(QStringLiteral("没有可撤销的操作。"));
        return;
    }

    const UndoType type = undo_type_;
    const NoteEvent event = undo_event_;
    clearUndo();

    if (type == UndoType::Delete) {
        if (note_store_restore(&store_, &event)) {
            saveNotes();
            reload();
            emit selectedEventChanged();
            appendOperationLog(QStringLiteral("undo_delete"), &event);
            setNotice(QStringLiteral("已撤销删除。"));
            return;
        }
    } else if (type == UndoType::Toggle) {
        NoteEvent *current = findEvent(event.id);
        if (current != nullptr) {
            *current = event;
            saveNotes();
            reloadModelChange();
            if (selected_event_id_ == event.id) {
                emit selectedEventChanged();
            }
            appendOperationLog(QStringLiteral("undo_toggle"), &event);
            setNotice(QStringLiteral("已撤销完成状态。"));
            return;
        }
    }

    setNotice(QStringLiteral("撤销失败，原事件已变化。"));
}

void NoteApp::selectEvent(int id, bool readOnly) {
    if (findEvent(id) == nullptr) {
        clearSelectedEvent();
        return;
    }
    if (selected_event_id_ == id && selected_event_read_only_ == readOnly) {
        return;
    }
    selected_event_id_ = id;
    selected_event_read_only_ = readOnly;
    emit selectedEventChanged();
}

void NoteApp::clearSelectedEvent() {
    if (selected_event_id_ == -1 && !selected_event_read_only_) {
        return;
    }
    selected_event_id_ = -1;
    selected_event_read_only_ = false;
    emit selectedEventChanged();
}

void NoteApp::saveSelectedEvent(const QString &text) {
    if (!hasSelectedEvent()) {
        setNotice(QStringLiteral("没有选中的事件。"));
        return;
    }
    if (selected_event_read_only_) {
        setNotice(QStringLiteral("自动收纳内容只读，不能在这里修改。"));
        return;
    }
    updateEvent(selected_event_id_, text);
}

void NoteApp::saveConfig() {
    config_save(&config_, config_path_);
    setNotice(QStringLiteral("设置已保存。"));
}

bool NoteApp::setEventRepeat(int id, const QString &repeat) {
    const QString normalized = repeat.trimmed().toLower();
    const QString value = normalized == QStringLiteral("none") ? QString() : normalized;
    const std::wstring wide = toWide(value);
    if (!note_store_set_repeat(&store_, id, wide.c_str())) {
        return false;
    }
    saveNotes();
    reloadModelChange();
    if (selected_event_id_ == id) {
        emit selectedEventChanged();
    }
    appendOperationLog(QStringLiteral("repeat"), findEventConst(id), value);
    setNotice(value.isEmpty() ? QStringLiteral("已关闭重复任务。") : QStringLiteral("已设置重复任务。"));
    return true;
}

bool NoteApp::exportJson() {
    return exportJsonToFile(QUrl::fromLocalFile(fromWide(data_dir_) + QStringLiteral("/notes-export.json")));
}

bool NoteApp::exportJsonToFile(const QUrl &fileUrl) {
    QString path;
    QString error;
    if (!BackupService::exportJson(store_, fileUrl, &path, &error)) {
        setNotice(error);
        return false;
    }
    appendOperationLog(QStringLiteral("export_json"), nullptr, QDir::toNativeSeparators(path));
    setNotice(QStringLiteral("JSON 已导出。"));
    return true;
}

bool NoteApp::importJson() {
    return importJsonFromFile(QUrl::fromLocalFile(fromWide(data_dir_) + QStringLiteral("/notes-export.json")));
}

bool NoteApp::importJsonFromFile(const QUrl &fileUrl) {
    QString path;
    QString error;
    if (!BackupService::importJson(fileUrl, &store_, &path, &error)) {
        setNotice(error);
        return false;
    }
    clearUndo();
    clearSelectedEvent();
    saveNotes();
    reload();
    appendOperationLog(QStringLiteral("import_json"), nullptr, QStringLiteral("%1 from %2").arg(store_.count).arg(QDir::toNativeSeparators(path)));
    setNotice(QStringLiteral("JSON 已导入。"));
    return true;
}

bool NoteApp::exportMarkdown() {
    return exportMarkdownToFile(QUrl::fromLocalFile(fromWide(data_dir_) + QStringLiteral("/notes-export.md")));
}

bool NoteApp::exportMarkdownToFile(const QUrl &fileUrl) {
    QString path;
    QString error;
    if (!BackupService::exportMarkdown(store_, fileUrl, &path, &error)) {
        setNotice(error);
        return false;
    }
    appendOperationLog(QStringLiteral("export_markdown"), nullptr, QDir::toNativeSeparators(path));
    setNotice(QStringLiteral("Markdown 已导出。"));
    return true;
}

int NoteApp::previewImportJsonEventCount(const QUrl &fileUrl) {
    QString error;
    const int count = BackupService::previewJsonEventCount(fileUrl, &error);
    if (count < 0) {
        setNotice(error);
    }
    return count;
}

QString NoteApp::summarize(const QString &requirement) {
    if (requirement.trimmed().isEmpty()) {
        return QStringLiteral("总结要求不能为空。");
    }
    if (rows_.isEmpty()) {
        return QStringLiteral("当前阶段还没有事件，先写点东西再总结。");
    }
    const QString notes = summaryNotes();
    const QString result = summarizeWithConfig(config_, requirement, notes);
    appendSummaryHistory(requirement.trimmed(), notes, result);
    return result;

}

void NoteApp::summarizeAsync(const QString &requirement) {
    const QString trimmed = requirement.trimmed();
    if (trimmed.isEmpty()) {
        emit summaryReady(QStringLiteral("总结要求不能为空。"));
        return;
    }
    if (rows_.isEmpty()) {
        emit summaryReady(QStringLiteral("当前阶段还没有事件，先写点东西再总结。"));
        return;
    }

    const AppConfig config = config_;
    const QString notes = summaryNotes();
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, trimmed, notes]() {
        const QString result = watcher->result();
        watcher->deleteLater();
        appendSummaryHistory(trimmed, notes, result);
        emit summaryReady(result);
    });
    watcher->setFuture(QtConcurrent::run([config, trimmed, notes]() {
        return summarizeWithConfig(config, trimmed, notes);
    }));
}

QString NoteApp::summaryNotes() const {
    QString notes = QStringLiteral("阶段：%1 %2\n").arg(stageLabel(), dateKey());
    for (const RowItem &row : rows_) {
        const NoteEvent *event = row.event;
        if (event == nullptr) continue;
        notes += QStringLiteral("%1 %2 %3\n")
            .arg(row.archive ? QStringLiteral("[自动收纳]") : QStringLiteral("[计划]"),
                 event->completed ? QStringLiteral("[已完成]") : QStringLiteral("[未完成]"),
                 fromWide(event->text));
    }
    return notes;
}

QString NoteApp::summarizeWithConfig(const AppConfig &config, const QString &requirement, const QString &notes) {
    wchar_t result[8192]{};
    wchar_t error[512]{};
    const std::wstring wide_requirement = toWide(requirement);
    const std::wstring wide_notes = toWide(notes);
    const int ok = ai_client_summarize(&config, wide_requirement.c_str(), wide_notes.c_str(),
                                       result, 8192, error, 512);
    if (!ok) {
        return fromWide(error);
    }
    return fromWide(result);
}

NoteEvent *NoteApp::eventAt(int row) const {
    return row >= 0 && row < rows_.size() ? rows_.at(row).event : nullptr;
}

NoteEvent *NoteApp::findEvent(int id) {
    for (int i = 0; i < store_.count; ++i) {
        if (store_.items[i].id == id) return &store_.items[i];
    }
    return nullptr;
}

const NoteEvent *NoteApp::findEventConst(int id) const {
    for (int i = 0; i < store_.count; ++i) {
        if (store_.items[i].id == id) return &store_.items[i];
    }
    return nullptr;
}

void NoteApp::syncSelectedEvent() {
    if (selected_event_id_ != -1 && findEventConst(selected_event_id_) == nullptr) {
        clearSelectedEvent();
    }
}

void NoteApp::reload() {
    beginResetModel();
    rows_ = buildRows();
    endResetModel();
    ++view_revision_;
    emit countsChanged();
    emit viewRevisionChanged();
}

void NoteApp::reloadModelChange() {
    const QVector<RowItem> next = buildRows();
    if (next.size() != rows_.size()) {
        reload();
        return;
    }

    auto idsFor = [](const QVector<RowItem> &rows) {
        QVector<int> ids;
        ids.reserve(rows.size());
        for (const RowItem &row : rows) {
            ids.append(row.event != nullptr ? row.event->id : -1);
        }
        return ids;
    };

    const QVector<int> current_ids = idsFor(rows_);
    const QVector<int> next_ids = idsFor(next);
    if (current_ids != next_ids) {
        for (int from = 0; from < current_ids.size(); ++from) {
            const int id = current_ids.at(from);
            const int to = next_ids.indexOf(id);
            if (to < 0 || to == from) {
                continue;
            }

            QVector<int> moved = current_ids;
            moved.removeAt(from);
            moved.insert(to, id);
            if (moved == next_ids) {
                const int destination = to > from ? to + 1 : to;
                beginMoveRows(QModelIndex(), from, from, QModelIndex(), destination);
                rows_ = next;
                endMoveRows();
                if (!rows_.isEmpty()) {
                    emit dataChanged(index(0, 0), index(rows_.size() - 1, 0));
                }
                ++view_revision_;
                emit countsChanged();
                emit viewRevisionChanged();
                return;
            }
        }
    }

    emit layoutAboutToBeChanged();
    rows_ = next;
    emit layoutChanged();
    if (!rows_.isEmpty()) {
        emit dataChanged(index(0, 0), index(rows_.size() - 1, 0));
    }
    ++view_revision_;
    emit countsChanged();
    emit viewRevisionChanged();
}

QVector<NoteApp::RowItem> NoteApp::buildRows() {
    NoteView::BuildRequest request;
    request.stage = stage_;
    request.date_key = dateKey();
    request.search_query = search_query_;
    request.search_completion_filter = search_completion_filter_;
    return NoteView::buildRows(&store_, request);
}

void NoteApp::saveNotes() {
    note_store_save_sqlite(&store_, sqlite_path_);
}

void NoteApp::appendOperationLog(const QString &action, const NoteEvent *event, const QString &detail) {
    QFile file(fromWide(operation_log_path_));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QJsonObject item;
    item.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    item.insert(QStringLiteral("action"), action);
    if (event != nullptr) {
        item.insert(QStringLiteral("eventId"), event->id);
        item.insert(QStringLiteral("stage"), event->stage);
        item.insert(QStringLiteral("dateKey"), fromWide(event->date_key));
        item.insert(QStringLiteral("text"), fromWide(event->text));
        item.insert(QStringLiteral("completed"), event->completed != 0);
    }
    if (!detail.isEmpty()) {
        item.insert(QStringLiteral("detail"), detail);
    }
    file.write(QJsonDocument(item).toJson(QJsonDocument::Compact));
    file.write("\n");
}

void NoteApp::appendSummaryHistory(const QString &requirement, const QString &notes, const QString &result) {
    QFile file(fromWide(summary_history_path_));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out << "## " << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n\n";
    out << "### 要求\n\n" << requirement << "\n\n";
    out << "### 结果\n\n" << result << "\n\n";
    out << "### 输入\n\n```text\n" << notes << "```\n\n";
}

void NoteApp::updateDateKey() {
    const QDate now = QDate::currentDate();
    if (stage_ == NOTE_STAGE_DAY) {
        const std::wstring value = toWide(now.toString(QStringLiteral("yyyy-MM-dd")));
        wcsncpy(date_key_, value.c_str(), NOTE_KEY_MAX - 1);
    } else if (stage_ == NOTE_STAGE_WEEK) {
        int year = 0;
        const int week = now.weekNumber(&year);
        const std::wstring value = toWide(QStringLiteral("%1-W%2")
            .arg(year, 4, 10, QLatin1Char('0'))
            .arg(week, 2, 10, QLatin1Char('0')));
        wcsncpy(date_key_, value.c_str(), NOTE_KEY_MAX - 1);
    } else if (stage_ == NOTE_STAGE_MONTH) {
        const std::wstring value = toWide(now.toString(QStringLiteral("yyyy-MM")));
        wcsncpy(date_key_, value.c_str(), NOTE_KEY_MAX - 1);
    } else {
        const std::wstring value = toWide(now.toString(QStringLiteral("yyyy")));
        wcsncpy(date_key_, value.c_str(), NOTE_KEY_MAX - 1);
    }
    date_key_[NOTE_KEY_MAX - 1] = L'\0';
}

void NoteApp::initPaths() {
    const QString overrideDir = qEnvironmentVariable("STICKY_NOTES_DATA_DIR");
    const QString dataDir = overrideDir.trimmed().isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/data")
        : overrideDir;
    const std::wstring data = toWide(QDir::toNativeSeparators(dataDir));
    const std::wstring notes = toWide(QDir::toNativeSeparators(dataDir + QStringLiteral("/notes.db.txt")));
    const std::wstring sqlite = toWide(QDir::toNativeSeparators(dataDir + QStringLiteral("/notes.sqlite")));
    const std::wstring config = toWide(QDir::toNativeSeparators(dataDir + QStringLiteral("/config.ini")));
    const std::wstring operationLog = toWide(QDir::toNativeSeparators(dataDir + QStringLiteral("/operations.jsonl")));
    const std::wstring summaryHistory = toWide(QDir::toNativeSeparators(dataDir + QStringLiteral("/summary-history.md")));
    wcsncpy(data_dir_, data.c_str(), MAX_PATH - 1);
    wcsncpy(notes_path_, notes.c_str(), MAX_PATH - 1);
    wcsncpy(sqlite_path_, sqlite.c_str(), MAX_PATH - 1);
    wcsncpy(config_path_, config.c_str(), MAX_PATH - 1);
    wcsncpy(operation_log_path_, operationLog.c_str(), MAX_PATH - 1);
    wcsncpy(summary_history_path_, summaryHistory.c_str(), MAX_PATH - 1);
    data_dir_[MAX_PATH - 1] = L'\0';
    notes_path_[MAX_PATH - 1] = L'\0';
    sqlite_path_[MAX_PATH - 1] = L'\0';
    config_path_[MAX_PATH - 1] = L'\0';
    operation_log_path_[MAX_PATH - 1] = L'\0';
    summary_history_path_[MAX_PATH - 1] = L'\0';
}

void NoteApp::setNotice(const QString &value) {
    if (notice_ == value) return;
    notice_ = value;
    emit noticeChanged();
}

void NoteApp::rememberUndo(UndoType type, const NoteEvent &event) {
    undo_type_ = type;
    undo_event_ = event;
    undo_expires_at_ = QDateTime::currentMSecsSinceEpoch() + 5000;
    emit undoChanged();
}

void NoteApp::clearUndo() {
    if (undo_type_ == UndoType::None && undo_expires_at_ == 0) {
        return;
    }
    undo_type_ = UndoType::None;
    memset(&undo_event_, 0, sizeof(undo_event_));
    undo_expires_at_ = 0;
    emit undoChanged();
}

QDate NoteApp::nextRepeatDate(const NoteEvent &event) const {
    QDate date = QDate::fromString(fromWide(event.date_key), QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid()) {
        date = QDate::currentDate();
    }
    const QString repeat = fromWide(event.repeat);
    if (repeat == QStringLiteral("daily")) return date.addDays(1);
    if (repeat == QStringLiteral("weekly")) return date.addDays(7);
    if (repeat == QStringLiteral("monthly")) return date.addMonths(1);
    if (repeat == QStringLiteral("yearly")) return date.addYears(1);
    return {};
}

void NoteApp::createNextRepeatIfNeeded(const NoteEvent &event) {
    if (event.repeat[0] == L'\0' || event.stage != NOTE_STAGE_DAY || event.completed) {
        return;
    }
    const QDate next_date = nextRepeatDate(event);
    if (!next_date.isValid()) {
        return;
    }
    const QString next_key = next_date.toString(QStringLiteral("yyyy-MM-dd"));
    const QString text = fromWide(event.text);
    for (int i = 0; i < store_.count; ++i) {
        const NoteEvent &candidate = store_.items[i];
        if (candidate.stage == NOTE_STAGE_DAY &&
            fromWide(candidate.date_key) == next_key &&
            fromWide(candidate.text) == text) {
            return;
        }
    }
    const std::wstring key = toWide(next_key);
    const std::wstring wide_text = toWide(text);
    NoteEvent *next = note_store_add(&store_, NOTE_STAGE_DAY, key.c_str(), wide_text.c_str());
    if (next != nullptr) {
        wcsncpy(next->repeat, event.repeat, NOTE_REPEAT_MAX - 1);
        next->repeat[NOTE_REPEAT_MAX - 1] = L'\0';
    }
}

QString NoteApp::fromWide(const wchar_t *value) {
    return value == nullptr ? QString() : QString::fromWCharArray(value);
}

std::wstring NoteApp::toWide(const QString &value) {
    return value.toStdWString();
}
