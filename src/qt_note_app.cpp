#include "qt_note_app.h"

#include "ai_client.h"
#include "backup_service.h"
#include "credential_store.h"
#include "local_profile.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
#include <QTimeZone>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <utility>

namespace {

constexpr qint64 kMaximumOperationLogBytes = 50LL * 1024LL * 1024LL;

QString stageName(NotebookStage stage) {
    switch (stage) {
        case NotebookStage::Day: return QStringLiteral("每天");
        case NotebookStage::Week: return QStringLiteral("每周");
        case NotebookStage::Month: return QStringLiteral("每月");
        case NotebookStage::Year: return QStringLiteral("每年");
    }
    return {};
}

QString repeatName(NotebookRepeat repeat) {
    switch (repeat) {
        case NotebookRepeat::None: return {};
        case NotebookRepeat::Daily: return QStringLiteral("daily");
        case NotebookRepeat::Weekly: return QStringLiteral("weekly");
        case NotebookRepeat::Monthly: return QStringLiteral("monthly");
        case NotebookRepeat::Yearly: return QStringLiteral("yearly");
    }
    return {};
}

std::optional<NotebookRepeat> repeatFromName(const QString &value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("none")) return NotebookRepeat::None;
    if (normalized == QStringLiteral("daily")) return NotebookRepeat::Daily;
    if (normalized == QStringLiteral("weekly")) return NotebookRepeat::Weekly;
    if (normalized == QStringLiteral("monthly")) return NotebookRepeat::Monthly;
    if (normalized == QStringLiteral("yearly")) return NotebookRepeat::Yearly;
    return std::nullopt;
}

QString noteMeta(const NotebookNote &note, const QDate &today) {
    if (note.completed && note.completedAt > 0) {
        return QStringLiteral("完成 %1").arg(
            QDateTime::fromSecsSinceEpoch(note.completedAt).toString(QStringLiteral("MM-dd HH:mm")));
    }
    if (note.createdAt > 0) {
        const QDateTime created = QDateTime::fromSecsSinceEpoch(note.createdAt);
        return created.date() == today ? QStringLiteral("今天")
                                       : created.toString(QStringLiteral("MM-dd"));
    }
    return QStringLiteral("刚刚创建");
}

QString archiveMeta(const NotebookNote &note) {
    if (note.completed && note.completedAt > 0) {
        return QStringLiteral("来自 %1 · 完成 %2")
            .arg(note.dateKey,
                 QDateTime::fromSecsSinceEpoch(note.completedAt).toString(QStringLiteral("MM-dd HH:mm")));
    }
    return QStringLiteral("来自 %1").arg(note.dateKey);
}

QString searchMeta(const NotebookNote &note) {
    return QStringLiteral("%1 · %2 · %3")
        .arg(stageName(note.stage), note.dateKey,
             note.completed ? QStringLiteral("已完成") : QStringLiteral("未完成"));
}

QString highlightedText(const QString &value, const QString &queryValue) {
    const QString text = value.toHtmlEscaped();
    const QString query = queryValue.trimmed().toHtmlEscaped();
    if (query.isEmpty()) return text;
    QString result;
    qsizetype cursor = 0;
    while (cursor < text.size()) {
        const qsizetype index = text.indexOf(query, cursor, Qt::CaseInsensitive);
        if (index < 0) {
            result += text.mid(cursor);
            break;
        }
        result += text.mid(cursor, index - cursor);
        result += QStringLiteral("<mark>") + text.mid(index, query.size()) + QStringLiteral("</mark>");
        cursor = index + query.size();
    }
    return result;
}

bool belongsToArchive(const NotebookNote &note, NotebookStage stage, const QString &dateKey) {
    if (note.stage != NotebookStage::Day || stage == NotebookStage::Day) return false;
    const QDate date = QDate::fromString(note.dateKey, QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid()) return false;
    if (stage == NotebookStage::Week) {
        int year = 0;
        const int week = date.weekNumber(&year);
        return QStringLiteral("%1-W%2").arg(year, 4, 10, QLatin1Char('0'))
            .arg(week, 2, 10, QLatin1Char('0')) == dateKey;
    }
    if (stage == NotebookStage::Month) return date.toString(QStringLiteral("yyyy-MM")) == dateKey;
    return date.toString(QStringLiteral("yyyy")) == dateKey;
}

bool writeAtomicFile(const QString &path, const QByteArray &bytes, QString *error) {
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法写入 %1：%2")
                         .arg(QDir::toNativeSeparators(path), file.errorString());
        }
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        if (error != nullptr) {
            *error = QStringLiteral("文件写入不完整：%1").arg(file.errorString());
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("无法原子提交文件：%1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

} // namespace

NoteApp::NoteApp(QObject *parent)
    : NoteApp(std::make_shared<SystemClock>(), parent) {
}

NoteApp::NoteApp(std::shared_ptr<Clock> clock, QObject *parent)
    : QAbstractListModel(parent),
      clock_(clock ? std::move(clock) : std::make_shared<SystemClock>()) {
    initPaths();
    QString profile_error;
    if (!LocalProfile::ensureAndMigrate(LocalProfile::resolve(), &profile_error)) {
        setNotice(profile_error);
    }
    QDir().mkpath(data_dir_);
    const bool needs_legacy_import = !QFileInfo::exists(sqlite_path_) && QFileInfo::exists(notes_path_);
    const NotebookError open_error = notebook_.open(sqlite_path_);
    if (open_error.isError()) {
        setNotice(QStringLiteral("便签数据库无法读取：%1").arg(open_error.message));
    } else if (needs_legacy_import) {
        const NotebookMutation migration = notebook_.importLegacyFile(notes_path_);
        if (!migration.ok()) {
            setNotice(QStringLiteral("旧便签迁移失败，已保留原文件：%1").arg(migration.error.message));
        }
    }
    config_ = defaultConfig();
    persisted_config_ = config_;
    QString config_error;
    if (loadConfig(config_path_, &config_, &config_error)) {
        persisted_config_ = config_;
    } else {
        if (!QFileInfo::exists(config_path_)) {
            config_ = defaultConfig();
            if (!::saveConfig(config_path_, config_, &config_error)) {
                setNotice(QStringLiteral("设置文件无法创建，当前设置仅在本次运行生效：%1")
                              .arg(config_error));
            } else {
                persisted_config_ = config_;
            }
        } else {
            config_read_only_ = true;
            setNotice(QStringLiteral("设置文件无法加载，已保留原文件：%1")
                          .arg(config_error));
        }
    }
    if (!config_.legacyApiKey.isEmpty()) {
        QString credential_error;
        if (credential_store_->writeSecret(config_.legacyApiKey,
                                           &credential_error)) {
            config_.legacyApiKey.clear();
            if (!::saveConfig(config_path_, config_, &config_error)) {
                setNotice(QStringLiteral("API Key 已迁入凭据库，但旧设置文件清理失败。"));
            } else {
                persisted_config_ = config_;
            }
        } else {
            setNotice(QStringLiteral("旧 API Key 尚未迁入 Windows 凭据库：%1").arg(credential_error));
        }
    }
    updateDateKey();
    reload();
    date_refresh_timer_.setSingleShot(true);
    connect(&date_refresh_timer_, &QTimer::timeout, this, [this]() {
        refreshDateIfNeeded();
        scheduleDateRefresh();
    });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            refreshDateIfNeeded();
            scheduleDateRefresh();
        }
    });
    scheduleDateRefresh();
}

NoteApp::~NoteApp() = default;

int NoteApp::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : rows_.size();
}

QVariant NoteApp::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }
    const RowItem &row = rows_.at(index.row());
    const NotebookNote &event = row.note;
    switch (role) {
        case IdRole: return event.id;
        case TextRole: return event.text;
        case CompletedRole: return event.completed;
        case MetaRole: return row.meta;
        case SectionRole: return row.section;
        case SectionFirstRole: return row.section_first;
        case ReadOnlyRole: return row.archive;
        case ArchiveRole: return row.archive;
        case HighlightedTextRole: return row.highlighted_text;
        case RepeatRole: return repeatName(event.repeat);
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
    if (value < static_cast<int>(NotebookStage::Day) ||
        value > static_cast<int>(NotebookStage::Year) || value == static_cast<int>(stage_)) {
        return;
    }
    stage_ = static_cast<NotebookStage>(value);
    updateDateKey();
    reload();
    emit stageChanged();
}

QString NoteApp::stageLabel() const {
    return stageName(stage_);
}

QString NoteApp::dateKey() const {
    return date_key_;
}

int NoteApp::completedCount() const {
    int done = 0;
    for (const RowItem &row : rows_) {
        if (!row.archive && row.note.completed) ++done;
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
           undo_expires_at_ >= clock_->nowMSecsSinceEpoch();
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
    return findEvent(selected_event_id_).has_value();
}

int NoteApp::selectedEventId() const {
    return hasSelectedEvent() ? selected_event_id_ : -1;
}

QString NoteApp::selectedEventText() const {
    const auto event = findEvent(selected_event_id_);
    return event ? event->text : QString();
}

QString NoteApp::selectedEventMeta() const {
    const auto event = findEvent(selected_event_id_);
    if (!event) {
        return {};
    }
    return selected_event_read_only_ ? archiveMeta(*event) : noteMeta(*event, clock_->now().date());
}

QString NoteApp::selectedEventRepeat() const {
    const auto event = findEvent(selected_event_id_);
    return event ? repeatName(event->repeat) : QString();
}

bool NoteApp::selectedEventReadOnly() const {
    return hasSelectedEvent() && selected_event_read_only_;
}

QString NoteApp::apiUrl() const {
    return config_.apiUrl;
}

void NoteApp::setApiUrl(const QString &value) {
    if (config_.apiUrl == value) {
        return;
    }
    config_.apiUrl = value;
    emit configChanged();
}

bool NoteApp::hasApiKey() const {
    return credential_store_ != nullptr && credential_store_->hasSecret();
}

QString NoteApp::modelName() const {
    return config_.model;
}

void NoteApp::setModelName(const QString &value) {
    if (config_.model == value) {
        return;
    }
    config_.model = value;
    emit configChanged();
}

bool NoteApp::allowLocalHttp() const {
    return config_.allowLocalHttp;
}

void NoteApp::setAllowLocalHttp(bool value) {
    if (allowLocalHttp() == value) {
        return;
    }
    config_.allowLocalHttp = value;
    emit configChanged();
}

bool NoteApp::reduceMotion() const {
    return config_.reduceMotion;
}

void NoteApp::setReduceMotion(bool value) {
    if (reduceMotion() == value) {
        return;
    }
    config_.reduceMotion = value;
    emit configChanged();
}

QString NoteApp::uiFontFamily() const {
    return config_.uiFontFamily;
}

void NoteApp::setUiFontFamily(const QString &value) {
    const QString trimmed = value.trimmed().isEmpty() ? QStringLiteral("Microsoft YaHei UI") : value.trimmed();
    if (uiFontFamily() == trimmed) {
        return;
    }
    config_.uiFontFamily = trimmed;
    emit configChanged();
}

int NoteApp::uiFontSize() const {
    return config_.uiFontSize;
}

void NoteApp::setUiFontSize(int value) {
    const int normalized = value >= 10 && value <= 18 ? value : 14;
    if (config_.uiFontSize == normalized) {
        return;
    }
    config_.uiFontSize = normalized;
    emit configChanged();
}

QStringList NoteApp::uiFontFamilies() const {
    QStringList families = QFontDatabase::families();
    const QString current = uiFontFamily().trimmed().isEmpty()
        ? QStringLiteral("Microsoft YaHei UI")
        : uiFontFamily().trimmed();

    if (families.isEmpty()) {
        families << current;
    } else if (!families.contains(current, Qt::CaseInsensitive)) {
        families.prepend(current);
    }

    families.removeDuplicates();
    return families;
}

QString NoteApp::dataDirectory() const {
    return QDir::toNativeSeparators(data_dir_);
}

QString NoteApp::aiState() const {
    return ai_state_;
}

QString NoteApp::aiError() const {
    return ai_error_;
}

bool NoteApp::aiBusy() const {
    return ai_state_ == QStringLiteral("running");
}

void NoteApp::addEvent(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    NotebookDraft draft;
    draft.stage = stage_;
    draft.dateKey = date_key_;
    draft.text = trimmed;
    const NotebookMutation mutation =
        notebook_.add(draft, clock_->now().toSecsSinceEpoch());
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.add"), mutation,
                              QStringLiteral("便签创建失败，请检查内容后重试。"));
        return;
    }
    reload();
    setNotice(QStringLiteral("已添加新便签。"));
    const auto added = findEvent(mutation.noteId);
    appendOperationLog(QStringLiteral("add"), added ? &*added : nullptr);
}

void NoteApp::toggleEvent(int id) {
    const auto snapshot = findEvent(id);
    if (!snapshot) {
        return;
    }
    const NotebookMutation mutation =
        notebook_.toggle(id, clock_->now().toSecsSinceEpoch());
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.toggle"), mutation,
                              QStringLiteral("完成状态未保存，原状态保持不变。"));
        return;
    }
    rememberUndo(UndoType::Toggle, *snapshot,
                 mutation.derivedNoteIds.isEmpty()
                     ? -1
                     : mutation.derivedNoteIds.first());
    reloadModelChange();
    if (selected_event_id_ == id) {
        emit selectedEventChanged();
    }
    const auto changed = findEvent(id);
    appendOperationLog(QStringLiteral("toggle"), changed ? &*changed : nullptr);
    setNotice(QStringLiteral("已更新完成状态，可按 Ctrl+Z 撤销。"));
}

void NoteApp::deleteEvent(int id) {
    const auto snapshot = findEvent(id);
    if (!snapshot) {
        return;
    }
    const NotebookMutation mutation = notebook_.remove(id);
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.delete"), mutation,
                              QStringLiteral("便签删除失败，原便签保持不变。"));
        return;
    }
    rememberUndo(UndoType::Delete, *snapshot);
    reload();
    if (selected_event_id_ == id) {
        clearSelectedEvent();
    }
    appendOperationLog(QStringLiteral("delete"), &*snapshot);
    setNotice(QStringLiteral("已删除便签，可按 Ctrl+Z 撤销。"));
}

void NoteApp::updateEvent(int id, const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    NotebookUpdate update;
    update.text = trimmed;
    const NotebookMutation mutation =
        notebook_.update(id, update, clock_->now().toSecsSinceEpoch());
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.update"), mutation,
                              QStringLiteral("便签修改失败，原内容保持不变。"));
        return;
    }
    clearUndo();
    reloadModelChange();
    if (selected_event_id_ == id) {
        emit selectedEventChanged();
    }
    const auto changed = findEvent(id);
    appendOperationLog(QStringLiteral("update"), changed ? &*changed : nullptr);
}

void NoteApp::toggleAll() {
    NotebookQuery query;
    query.stage = stage_;
    query.dateKey = date_key_;
    const NotebookMutation mutation =
        notebook_.bulk(NotebookBulkAction::ToggleCompletion, query,
                       clock_->now().toSecsSinceEpoch());
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.toggle_all"), mutation,
                              QStringLiteral("批量完成失败，原状态保持不变。"));
        return;
    }
    clearUndo();
    reloadModelChange();
    appendOperationLog(QStringLiteral("toggle_all"), nullptr, QStringLiteral("%1 %2").arg(stageLabel(), dateKey()));
}

void NoteApp::clearCompletedCurrent() {
    NotebookQuery query;
    query.stage = stage_;
    query.dateKey = date_key_;
    query.completion = NotebookCompletion::Completed;
    const NotebookMutation mutation =
        notebook_.bulk(NotebookBulkAction::Remove, query);
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.clear_completed"), mutation,
                              QStringLiteral("清理失败，原便签保持不变。"));
        return;
    }
    if (mutation.affected > 0) {
        clearUndo();
        reload();
        syncSelectedEvent();
        appendOperationLog(QStringLiteral("clear_completed_current"), nullptr,
                           QString::number(mutation.affected));
        setNotice(QStringLiteral("已删除当前阶段的已完成便签。"));
    } else {
        setNotice(QStringLiteral("当前阶段没有已完成便签。"));
    }
}

void NoteApp::clearCurrentStage() {
    NotebookQuery query;
    query.stage = stage_;
    query.dateKey = date_key_;
    const NotebookMutation mutation =
        notebook_.bulk(NotebookBulkAction::Remove, query);
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.clear_stage"), mutation,
                              QStringLiteral("清空失败，原便签保持不变。"));
        return;
    }
    if (mutation.affected > 0) {
        clearUndo();
        reload();
        syncSelectedEvent();
        appendOperationLog(QStringLiteral("clear_current_stage"), nullptr,
                           QString::number(mutation.affected));
        setNotice(QStringLiteral("已清空当前便签阶段。"));
    } else {
        setNotice(QStringLiteral("当前阶段没有可清理的便签。"));
    }
}

void NoteApp::clearAllNotes() {
    const NotebookMutation mutation =
        notebook_.bulk(NotebookBulkAction::Remove, NotebookQuery{});
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.clear_all"), mutation,
                              QStringLiteral("清空失败，全部便签保持不变。"));
        return;
    }
    clearUndo();
    clearSelectedEvent();
    reload();
    appendOperationLog(QStringLiteral("clear_all"));
    setNotice(QStringLiteral("已清空全部便签。"));
}

void NoteApp::clearCompletedAll() {
    NotebookQuery query;
    query.completion = NotebookCompletion::Completed;
    const NotebookMutation mutation =
        notebook_.bulk(NotebookBulkAction::Remove, query);
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.clear_completed_all"), mutation,
                              QStringLiteral("清理失败，原便签保持不变。"));
        return;
    }
    if (mutation.affected > 0) {
        clearUndo();
        reload();
        syncSelectedEvent();
        appendOperationLog(QStringLiteral("clear_completed_all"), nullptr,
                           QString::number(mutation.affected));
        setNotice(QStringLiteral("已删除全部已完成便签。"));
    } else {
        setNotice(QStringLiteral("没有已完成便签可清理。"));
    }
}

void NoteApp::undoLastAction() {
    if (!canUndo()) {
        clearUndo();
        setNotice(QStringLiteral("没有可撤销的操作。"));
        return;
    }

    const UndoType type = undo_type_;
    const NotebookNote event = undo_event_;
    const int derived_event_id = undo_derived_event_id_;
    clearUndo();

    NotebookMutation mutation;
    if (type == UndoType::Delete) {
        mutation = notebook_.restore(event);
        if (mutation.ok()) {
            reload();
            emit selectedEventChanged();
            appendOperationLog(QStringLiteral("undo_delete"), &event);
            setNotice(QStringLiteral("已撤销删除。"));
            return;
        }
    } else if (type == UndoType::Toggle) {
        const QList<int> derived_ids =
            derived_event_id > 0 ? QList<int>{derived_event_id} : QList<int>{};
        mutation = notebook_.restoreReplacing(event, derived_ids);
        if (mutation.ok()) {
            reloadModelChange();
            if (selected_event_id_ == event.id) {
                emit selectedEventChanged();
            }
            appendOperationLog(QStringLiteral("undo_toggle"), &event);
            setNotice(QStringLiteral("已撤销完成状态。"));
            return;
        }
    }

    reportMutationFailure(QStringLiteral("undo"), mutation,
                          QStringLiteral("撤销失败，原便签保持不变。"));
}

void NoteApp::selectEvent(int id, bool readOnly) {
    if (!findEvent(id)) {
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
        setNotice(QStringLiteral("没有选中的便签。"));
        return;
    }
    if (selected_event_read_only_) {
        setNotice(QStringLiteral("自动收纳内容只读，不能在这里修改。"));
        return;
    }
    updateEvent(selected_event_id_, text);
}

bool NoteApp::saveConfig() {
    auto rollbackDraft = [this]() {
        if (!(config_ == persisted_config_)) {
            config_ = persisted_config_;
            emit configChanged();
        }
    };
    if (config_read_only_) {
        rollbackDraft();
        const QString message =
            QStringLiteral("设置文件版本不受支持或无法安全读取，未覆盖原文件。");
        setNotice(message);
        emit operationFailed(QStringLiteral("config.save"), message, true);
        return false;
    }
    QString error;
    if (!::saveConfig(config_path_, config_, &error)) {
        rollbackDraft();
        const QString message =
            QStringLiteral("设置保存失败，原设置文件保持不变：%1").arg(error);
        setNotice(message);
        emit operationFailed(QStringLiteral("config.save"), message, true);
        return false;
    }
    persisted_config_ = config_;
    setNotice(QStringLiteral("设置已安全保存。"));
    return true;
}

bool NoteApp::saveSettings(const QString &newApiKey) {
    const AppConfig original = persisted_config_;
    if (!saveConfig()) {
        return false;
    }
    if (newApiKey.trimmed().isEmpty()) {
        return true;
    }

    QString credentialError;
    if (credential_store_ != nullptr &&
        credential_store_->writeSecret(newApiKey, &credentialError)) {
        emit credentialsChanged();
        setNotice(QStringLiteral(
            "设置已保存，API Key 已写入 Windows 凭据库。"));
        return true;
    }

    QString rollbackError;
    const bool rolledBack = ::saveConfig(config_path_, original, &rollbackError);
    config_ = original;
    persisted_config_ = original;
    emit configChanged();
    const QString message =
        rolledBack
            ? (credentialError.isEmpty()
                   ? QStringLiteral("API Key 保存失败，设置修改已回滚。")
                   : QStringLiteral("%1，设置修改已回滚。").arg(credentialError))
            : QStringLiteral(
                  "API Key 保存失败，设置文件回滚也失败：%1")
                  .arg(rollbackError);
    setNotice(message);
    emit operationFailed(QStringLiteral("settings.save"), message,
                         rolledBack);
    return false;
}

bool NoteApp::replaceApiKey(const QString &value) {
    if (credential_store_ == nullptr || value.trimmed().isEmpty()) {
        const QString message = QStringLiteral("请输入有效的 API Key。" );
        setNotice(message);
        emit operationFailed(QStringLiteral("credential.invalid"), message, true);
        return false;
    }
    QString error;
    if (!credential_store_->writeSecret(value, &error)) {
        const QString message = error.isEmpty() ? QStringLiteral("API Key 保存失败。") : error;
        setNotice(message);
        emit operationFailed(QStringLiteral("credential.write"), message, true);
        return false;
    }
    emit credentialsChanged();
    setNotice(QStringLiteral("API Key 已保存到 Windows 凭据库。"));
    return true;
}

bool NoteApp::clearApiKey() {
    if (credential_store_ == nullptr) {
        return false;
    }
    QString error;
    if (!credential_store_->clearSecret(&error)) {
        const QString message = error.isEmpty() ? QStringLiteral("API Key 删除失败。") : error;
        setNotice(message);
        emit operationFailed(QStringLiteral("credential.delete"), message, true);
        return false;
    }
    emit credentialsChanged();
    setNotice(QStringLiteral("API Key 已从 Windows 凭据库删除。"));
    return true;
}

bool NoteApp::setEventRepeat(int id, const QString &repeat) {
    const auto value = repeatFromName(repeat);
    if (!value.has_value()) {
        const QString message = QStringLiteral("不支持的重复规则。");
        setNotice(message);
        emit operationFailed(QStringLiteral("note.repeat.invalid"), message, true);
        return false;
    }
    NotebookUpdate update;
    update.repeat = *value;
    const NotebookMutation mutation =
        notebook_.update(id, update, clock_->now().toSecsSinceEpoch());
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("note.repeat"), mutation,
                              QStringLiteral("重复规则未保存，原设置保持不变。"));
        return false;
    }
    reloadModelChange();
    if (selected_event_id_ == id) {
        emit selectedEventChanged();
    }
    const auto changed = findEvent(id);
    appendOperationLog(QStringLiteral("repeat"), changed ? &*changed : nullptr,
                       repeatName(*value));
    setNotice(*value == NotebookRepeat::None ? QStringLiteral("已关闭重复任务。")
                                             : QStringLiteral("已设置重复任务。"));
    return true;
}

bool NoteApp::exportJson() {
    return exportJsonToFile(QUrl::fromLocalFile(QDir(data_dir_).filePath(QStringLiteral("stickies-export.json"))));
}

bool NoteApp::exportJsonToFile(const QUrl &fileUrl) {
    const auto snapshot = notebook_.snapshot();
    if (!snapshot.ok()) {
        setNotice(snapshot.error.message);
        emit operationFailed(QStringLiteral("backup.export_json"),
                             snapshot.error.message, true);
        return false;
    }
    QString path;
    QString error;
    if (!BackupService::exportJson(snapshot.value, fileUrl, &path, &error)) {
        setNotice(error);
        emit operationFailed(QStringLiteral("backup.export_json"), error, true);
        return false;
    }
    appendOperationLog(QStringLiteral("export_json"), nullptr, QDir::toNativeSeparators(path));
    setNotice(QStringLiteral("JSON 已导出。"));
    return true;
}

bool NoteApp::importJson() {
    return importJsonFromFile(QUrl::fromLocalFile(QDir(data_dir_).filePath(QStringLiteral("stickies-export.json"))));
}

bool NoteApp::importJsonFromFile(const QUrl &fileUrl) {
    NotebookSnapshot imported;
    QString path;
    QString error;
    if (!BackupService::importJson(fileUrl, &imported, &path, &error)) {
        setNotice(error);
        emit operationFailed(QStringLiteral("backup.import_json"), error, true);
        return false;
    }
    const NotebookMutation mutation = notebook_.importSnapshot(imported);
    if (!mutation.ok()) {
        reportMutationFailure(QStringLiteral("backup.import_json"), mutation,
                              QStringLiteral("JSON 导入失败，原数据保持不变。"));
        return false;
    }
    clearUndo();
    clearSelectedEvent();
    reload();
    appendOperationLog(QStringLiteral("import_json"), nullptr,
                       QStringLiteral("%1 from %2")
                           .arg(mutation.affected)
                           .arg(QDir::toNativeSeparators(path)));
    setNotice(QStringLiteral("JSON 已导入。"));
    return true;
}

bool NoteApp::exportMarkdown() {
    return exportMarkdownToFile(QUrl::fromLocalFile(QDir(data_dir_).filePath(QStringLiteral("stickies-export.md"))));
}

bool NoteApp::exportMarkdownToFile(const QUrl &fileUrl) {
    const auto snapshot = notebook_.snapshot();
    if (!snapshot.ok()) {
        setNotice(snapshot.error.message);
        emit operationFailed(QStringLiteral("backup.export_markdown"),
                             snapshot.error.message, true);
        return false;
    }
    QString path;
    QString error;
    if (!BackupService::exportMarkdown(snapshot.value, fileUrl, &path, &error)) {
        setNotice(error);
        emit operationFailed(QStringLiteral("backup.export_markdown"), error,
                             true);
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
        return QStringLiteral("当前阶段还没有便签，先写点东西再总结。");
    }
    const QString notes = summaryNotes();
    const QPair<bool, QString> result =
        summarizeRequest(aiRequest(requirement.trimmed(), notes));
    if (result.first) {
        appendSummaryHistory(requirement.trimmed(), notes, result.second);
    }
    return result.second;

}

void NoteApp::summarizeAsync(const QString &requirement) {
    const QString trimmed = requirement.trimmed();
    if (trimmed.isEmpty()) {
        setAiState(QStringLiteral("error"), QStringLiteral("总结要求不能为空。"));
        emit summaryReady(QStringLiteral("总结要求不能为空。"));
        return;
    }
    const QString notes = summaryNotes();
    if (notes.trimmed().isEmpty()) {
        setAiState(QStringLiteral("error"), QStringLiteral("当前阶段还没有便签。"));
        emit summaryReady(QStringLiteral("当前阶段还没有便签，先写点东西再总结。"));
        return;
    }

    if (ai_cancel_token_) {
        ai_cancel_token_->store(true, std::memory_order_release);
    }
    const AiClientRequest request = aiRequest(trimmed, notes);
    const quint64 request_id = ++ai_request_id_;
    const auto cancel_token = std::make_shared<std::atomic_bool>(false);
    ai_cancel_token_ = cancel_token;
    setAiState(QStringLiteral("running"));
    auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, [this, watcher, trimmed, notes, request_id, cancel_token]() {
        const QPair<bool, QString> result = watcher->result();
        watcher->deleteLater();
        if (request_id != ai_request_id_) {
            return;
        }
        if (ai_cancel_token_ == cancel_token) {
            ai_cancel_token_.reset();
        }
        if (result.first) {
            appendSummaryHistory(trimmed, notes, result.second);
            setAiState(QStringLiteral("success"));
        } else {
            setAiState(QStringLiteral("error"), result.second);
        }
        emit summaryReady(result.second);
    });
    watcher->setFuture(QtConcurrent::run([request, cancel_token]() {
        return summarizeRequest(request, cancel_token);
    }));
}

void NoteApp::summarizeContextAsync(const QString &requirement, const QString &context) {
    const QString trimmed = requirement.trimmed();
    const QString notes = context.trimmed();
    if (trimmed.isEmpty()) {
        setAiState(QStringLiteral("error"), QStringLiteral("总结要求不能为空。"));
        emit summaryReady(QStringLiteral("总结要求不能为空。"));
        return;
    }
    if (notes.isEmpty()) {
        setAiState(QStringLiteral("error"), QStringLiteral("暂无可摘要内容。"));
        emit summaryReady(QStringLiteral("暂无可摘要内容，请先创建项目或便签。"));
        return;
    }

    if (ai_cancel_token_) {
        ai_cancel_token_->store(true, std::memory_order_release);
    }
    const AiClientRequest request = aiRequest(trimmed, notes);
    const quint64 request_id = ++ai_request_id_;
    const auto cancel_token = std::make_shared<std::atomic_bool>(false);
    ai_cancel_token_ = cancel_token;
    setAiState(QStringLiteral("running"));
    auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, [this, watcher, trimmed, notes, request_id, cancel_token]() {
        const QPair<bool, QString> result = watcher->result();
        watcher->deleteLater();
        if (request_id != ai_request_id_) {
            return;
        }
        if (ai_cancel_token_ == cancel_token) {
            ai_cancel_token_.reset();
        }
        if (result.first) {
            appendSummaryHistory(trimmed, notes, result.second);
            setAiState(QStringLiteral("success"));
        } else {
            setAiState(QStringLiteral("error"), result.second);
        }
        emit summaryReady(result.second);
    });
    watcher->setFuture(QtConcurrent::run([request, cancel_token]() {
        return summarizeRequest(request, cancel_token);
    }));
}

void NoteApp::cancelSummary() {
    if (!aiBusy()) {
        return;
    }
    if (ai_cancel_token_) {
        ai_cancel_token_->store(true, std::memory_order_release);
        ai_cancel_token_.reset();
    }
    ++ai_request_id_;
    setAiState(QStringLiteral("cancelled"));
    setNotice(QStringLiteral("AI 摘要请求已取消。"));
}

void NoteApp::refreshTemporalState() {
    refreshDateIfNeeded();
    scheduleDateRefresh();
}

bool NoteApp::readWorkspaceSnapshot(BackupService::WorkspaceSnapshot *snapshot,
                                    QString *error) const {
    if (snapshot == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("工作区快照目标不可用。");
        }
        return false;
    }
    const auto notebook_snapshot = notebook_.snapshot();
    if (!notebook_snapshot.ok()) {
        if (error != nullptr) {
            *error = notebook_snapshot.error.message;
        }
        return false;
    }

    BackupService::WorkspaceSnapshot result;
    result.createdAt = clock_->now().toUTC().toString(Qt::ISODateWithMs);
    result.appVersion = QCoreApplication::applicationVersion().trimmed().isEmpty()
                            ? QStringLiteral("unknown")
                            : QCoreApplication::applicationVersion();
    result.notes.reserve(notebook_snapshot.value.notes.size());
    for (const NotebookNote &note : notebook_snapshot.value.notes) {
        BackupService::WorkspaceNote item;
        item.id = note.id;
        item.stage = static_cast<int>(note.stage);
        item.dateKey = note.dateKey;
        item.text = note.text;
        item.completed = note.completed;
        item.completedAt = note.completedAt;
        item.createdAt = note.createdAt;
        item.updatedAt = note.updatedAt;
        item.repeat = repeatName(note.repeat);
        item.seriesId = note.seriesId;
        result.notes.append(std::move(item));
    }

    QFile summaries(summary_history_path_);
    if (summaries.exists()) {
        if (!summaries.open(QIODevice::ReadOnly)) {
            if (error != nullptr) {
                *error = QStringLiteral("无法读取摘要历史：%1").arg(summaries.errorString());
            }
            return false;
        }
        QJsonObject history;
        history.insert(QStringLiteral("format"), QStringLiteral("markdown"));
        history.insert(QStringLiteral("content"),
                       QString::fromUtf8(summaries.readAll()));
        result.summaries.append(history);
    }

    result.preferences.insert(QStringLiteral("apiUrl"), apiUrl());
    result.preferences.insert(QStringLiteral("model"), modelName());
    result.preferences.insert(QStringLiteral("allowLocalHttp"), allowLocalHttp());
    result.preferences.insert(QStringLiteral("reduceMotion"), reduceMotion());
    result.preferences.insert(QStringLiteral("uiFontFamily"), uiFontFamily());
    result.preferences.insert(QStringLiteral("uiFontSize"), uiFontSize());
    *snapshot = std::move(result);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool NoteApp::applyWorkspaceSnapshot(
    const BackupService::WorkspaceSnapshot &snapshot, QString *error) {
    if (config_read_only_) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "当前设置文件无法安全写入，工作区未发生改变。");
        }
        return false;
    }
    NotebookSnapshot imported;
    imported.schemaVersion = Notebook::CurrentSchemaVersion;
    imported.notes.reserve(snapshot.notes.size());
    for (const BackupService::WorkspaceNote &item : snapshot.notes) {
        if (item.stage < static_cast<int>(NotebookStage::Day) ||
            item.stage > static_cast<int>(NotebookStage::Year)) {
            if (error != nullptr) {
                *error = QStringLiteral("工作区包含无效便签阶段。");
            }
            return false;
        }
        const auto repeat = repeatFromName(item.repeat);
        if (!repeat.has_value()) {
            if (error != nullptr) {
                *error = QStringLiteral("工作区包含无效重复规则。");
            }
            return false;
        }
        NotebookNote note;
        note.id = item.id;
        note.stage = static_cast<NotebookStage>(item.stage);
        note.dateKey = item.dateKey;
        note.text = item.text;
        note.completed = item.completed;
        note.completedAt = item.completedAt;
        note.createdAt = item.createdAt;
        note.updatedAt = item.updatedAt;
        note.repeat = *repeat;
        note.seriesId = item.seriesId;
        imported.notes.append(std::move(note));
    }

    AppConfig imported_config = config_;
    auto copyConfigString = [&](const QString &key, QString *target,
                                qsizetype maximumLength) -> bool {
        const QJsonValue value = snapshot.preferences.value(key);
        if (value.isUndefined()) {
            return true;
        }
        if (!value.isString()) {
            if (error != nullptr) {
                *error = QStringLiteral("工作区偏好 %1 类型无效。").arg(key);
            }
            return false;
        }
        const QString stringValue = value.toString();
        if (stringValue.size() > maximumLength ||
            stringValue.contains(QLatin1Char('\n')) ||
            stringValue.contains(QLatin1Char('\r')) ||
            stringValue.contains(QChar::Null)) {
            if (error != nullptr) {
                *error = QStringLiteral("工作区偏好 %1 超过长度限制。").arg(key);
            }
            return false;
        }
        *target = stringValue;
        return true;
    };
    if (!copyConfigString(QStringLiteral("apiUrl"), &imported_config.apiUrl,
                          AppConfig::MaximumUrlLength) ||
        !copyConfigString(QStringLiteral("model"), &imported_config.model,
                          AppConfig::MaximumModelLength) ||
        !copyConfigString(QStringLiteral("uiFontFamily"),
                          &imported_config.uiFontFamily,
                          AppConfig::MaximumFontFamilyLength)) {
        return false;
    }
    if (imported_config.uiFontFamily.trimmed().isEmpty()) {
        imported_config.uiFontFamily = defaultConfig().uiFontFamily;
    }
    auto copyConfigBool = [&](const QString &key, bool *target) -> bool {
        const QJsonValue value = snapshot.preferences.value(key);
        if (value.isUndefined()) {
            return true;
        }
        if (!value.isBool()) {
            if (error != nullptr) {
                *error = QStringLiteral("工作区偏好 %1 类型无效。").arg(key);
            }
            return false;
        }
        *target = value.toBool();
        return true;
    };
    if (!copyConfigBool(QStringLiteral("allowLocalHttp"),
                        &imported_config.allowLocalHttp) ||
        !copyConfigBool(QStringLiteral("reduceMotion"),
                        &imported_config.reduceMotion)) {
        return false;
    }
    const QJsonValue font_size = snapshot.preferences.value(
        QStringLiteral("uiFontSize"));
    if (!font_size.isUndefined()) {
        const int value = font_size.toInt(-1);
        if (!font_size.isDouble() || value < 10 || value > 18 ||
            font_size.toDouble() != value) {
            if (error != nullptr) {
                *error = QStringLiteral("工作区字体大小无效。");
            }
            return false;
        }
        imported_config.uiFontSize = value;
    }

    QByteArray imported_history;
    for (const QJsonValue &value : snapshot.summaries) {
        if (!value.isObject()) {
            if (error != nullptr) {
                *error = QStringLiteral("工作区摘要历史格式无效。");
            }
            return false;
        }
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("format")).toString() !=
                QStringLiteral("markdown") ||
            !object.value(QStringLiteral("content")).isString()) {
            if (error != nullptr) {
                *error = QStringLiteral("工作区摘要历史版本不受支持。");
            }
            return false;
        }
        if (!imported_history.isEmpty()) {
            imported_history.append("\n");
        }
        imported_history.append(
            object.value(QStringLiteral("content")).toString().toUtf8());
    }

    const auto original_notebook = notebook_.snapshot();
    if (!original_notebook.ok()) {
        if (error != nullptr) {
            *error = original_notebook.error.message;
        }
        return false;
    }
    const AppConfig original_config = config_;
    QFile original_history_file(summary_history_path_);
    const bool original_history_existed = original_history_file.exists();
    QByteArray original_history;
    if (original_history_existed) {
        if (!original_history_file.open(QIODevice::ReadOnly)) {
            if (error != nullptr) {
                *error = QStringLiteral("无法读取当前摘要历史：%1")
                             .arg(original_history_file.errorString());
            }
            return false;
        }
        original_history = original_history_file.readAll();
        original_history_file.close();
    }

    auto rollback = [&]() {
        notebook_.importSnapshot(original_notebook.value);
        ::saveConfig(config_path_, original_config, nullptr);
        config_ = original_config;
        persisted_config_ = original_config;
        if (original_history_existed) {
            writeAtomicFile(summary_history_path_, original_history, nullptr);
        } else {
            QFile::remove(summary_history_path_);
        }
    };

    const NotebookMutation note_import = notebook_.importSnapshot(imported);
    if (!note_import.ok()) {
        if (error != nullptr) {
            *error = note_import.error.message;
        }
        return false;
    }
    QString config_error;
    if (!::saveConfig(config_path_, imported_config, &config_error)) {
        rollback();
        if (error != nullptr) {
            *error = QStringLiteral("工作区设置无法原子提交，已恢复原数据：%1")
                         .arg(config_error);
        }
        return false;
    }
    QString history_error;
    if (!writeAtomicFile(summary_history_path_, imported_history,
                         &history_error)) {
        rollback();
        if (error != nullptr) {
            *error = QStringLiteral("%1，已恢复原数据。").arg(history_error);
        }
        return false;
    }

    config_ = imported_config;
    persisted_config_ = imported_config;
    clearUndo();
    clearSelectedEvent();
    reload();
    emit configChanged();
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

QJsonObject NoteApp::diagnosticsSnapshot() const {
    QJsonObject result;
    result.insert(QStringLiteral("schemaVersion"),
                  Notebook::CurrentSchemaVersion);
    result.insert(QStringLiteral("noteCount"), notebook_.count());
    result.insert(QStringLiteral("portableMode"),
                  LocalProfile::resolve().portable);
    result.insert(QStringLiteral("reduceMotion"), reduceMotion());
    result.insert(QStringLiteral("localHttpEnabled"), allowLocalHttp());
    result.insert(QStringLiteral("databaseState"),
                  notebook_.isOpen() ? QStringLiteral("ok")
                                     : QStringLiteral("unavailable"));
#if defined(QT_DEBUG)
    result.insert(QStringLiteral("buildType"), QStringLiteral("debug"));
#else
    result.insert(QStringLiteral("buildType"), QStringLiteral("release"));
#endif
    return result;
}

QString NoteApp::summaryNotes() const {
    NotebookQuery request;
    request.stage = stage_;
    request.dateKey = date_key_;
    const auto outcome = notebook_.query(request);
    if (!outcome.ok()) {
        return {};
    }
    QList<NotebookNote> summary_notes = outcome.value;
    if (stage_ != NotebookStage::Day) {
        NotebookQuery archive_request;
        archive_request.stage = NotebookStage::Day;
        const auto archive = notebook_.query(archive_request);
        if (archive.ok()) {
            for (const NotebookNote &note : archive.value) {
                if (belongsToArchive(note, stage_, date_key_)) {
                    summary_notes.append(note);
                }
            }
        }
    }
    if (summary_notes.isEmpty()) {
        return {};
    }
    QString notes = QStringLiteral("阶段：%1 %2\n").arg(stageLabel(), dateKey());
    for (const NotebookNote &note : std::as_const(summary_notes)) {
        const bool archive = note.stage == NotebookStage::Day &&
                             stage_ != NotebookStage::Day;
        notes += QStringLiteral("%1 %2 %3\n")
            .arg(archive ? QStringLiteral("[自动收纳]") : QStringLiteral("[计划]"),
                 note.completed ? QStringLiteral("[已完成]") : QStringLiteral("[未完成]"),
                 note.text);
    }
    return notes;
}

QPair<bool, QString> NoteApp::summarizeRequest(
    const AiClientRequest &request,
    const std::shared_ptr<std::atomic_bool> &cancelToken) {
    const AiClientResult result =
        summarizeWithAi(request, cancelToken ? cancelToken.get() : nullptr);
    if (!result.ok) {
        return qMakePair(false, result.error);
    }
    return qMakePair(true, result.content);
}

const NotebookNote *NoteApp::eventAt(int row) const {
    return row >= 0 && row < rows_.size() ? &rows_.at(row).note : nullptr;
}

std::optional<NotebookNote> NoteApp::findEvent(int id) const {
    if (id <= 0) {
        return std::nullopt;
    }
    const auto outcome = notebook_.note(id);
    return outcome.ok() ? std::optional<NotebookNote>(outcome.value)
                        : std::nullopt;
}

void NoteApp::syncSelectedEvent() {
    if (selected_event_id_ != -1 && !findEvent(selected_event_id_)) {
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
            ids.append(row.note.id);
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
    QVector<RowItem> result;
    const QString search = search_query_.trimmed();
    if (!search.isEmpty()) {
        NotebookQuery request;
        request.searchText = search;
        if (search_completion_filter_ == 1) {
            request.completion = NotebookCompletion::Completed;
        } else if (search_completion_filter_ == 0) {
            request.completion = NotebookCompletion::Open;
        }
        const auto outcome = notebook_.query(request);
        if (!outcome.ok()) {
            return result;
        }
        result.reserve(outcome.value.size());
        for (const NotebookNote &note : outcome.value) {
            result.append(RowItem{note,
                                  false,
                                  QStringLiteral("搜索结果"),
                                  searchMeta(note),
                                  false,
                                  highlightedText(note.text, search)});
        }
        if (!result.isEmpty()) {
            result[0].section_first = true;
        }
        return result;
    }

    NotebookQuery request;
    request.stage = stage_;
    request.dateKey = date_key_;
    const auto current = notebook_.query(request);
    if (!current.ok()) {
        return result;
    }
    result.reserve(current.value.size());
    const QDate today = clock_->now().date();
    for (const NotebookNote &note : current.value) {
        result.append(RowItem{note,
                              false,
                              note.completed ? QStringLiteral("完成批注")
                                             : QStringLiteral("待处理"),
                              noteMeta(note, today),
                              false,
                              highlightedText(note.text, {})});
    }

    if (stage_ != NotebookStage::Day) {
        const qsizetype archive_start = result.size();
        NotebookQuery archive_request;
        archive_request.stage = NotebookStage::Day;
        const auto archive = notebook_.query(archive_request);
        if (archive.ok()) {
            for (const NotebookNote &note : archive.value) {
                if (!belongsToArchive(note, stage_, date_key_)) {
                    continue;
                }
                result.append(RowItem{note,
                                      true,
                                      QStringLiteral("自动收纳"),
                                      archiveMeta(note),
                                      false,
                                      highlightedText(note.text, {})});
            }
            std::sort(result.begin() + archive_start, result.end(),
                      [](const RowItem &left, const RowItem &right) {
                          if (left.note.dateKey != right.note.dateKey) {
                              return left.note.dateKey > right.note.dateKey;
                          }
                          if (left.note.completed != right.note.completed) {
                              return !left.note.completed;
                          }
                          const qint64 left_time =
                              left.note.completed && left.note.completedAt > 0
                                  ? left.note.completedAt
                                  : left.note.createdAt;
                          const qint64 right_time =
                              right.note.completed && right.note.completedAt > 0
                                  ? right.note.completedAt
                                  : right.note.createdAt;
                          return left_time != right_time
                                     ? left_time > right_time
                                     : left.note.id > right.note.id;
                      });
        }
    }

    QString previous_section;
    for (RowItem &row : result) {
        row.section_first = row.section != previous_section;
        previous_section = row.section;
    }
    return result;
}

void NoteApp::reportMutationFailure(const QString &code,
                                    const NotebookMutation &mutation,
                                    const QString &fallbackMessage) {
    const QString message = mutation.error.message.trimmed().isEmpty()
                                ? fallbackMessage
                                : mutation.error.message;
    setNotice(message);
    emit operationFailed(code, message, true);
}

void NoteApp::appendOperationLog(const QString &action, const NotebookNote *event, const QString &detail) {
    if (QFileInfo(operation_log_path_).size() >=
        kMaximumOperationLogBytes - 16 * 1024) {
        if (!writeAtomicFile(operation_log_path_, {}, nullptr)) {
            return;
        }
    }
    QFile file(operation_log_path_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QJsonObject item;
    item.insert(QStringLiteral("timestamp"), clock_->now().toUTC().toString(Qt::ISODate));
    item.insert(QStringLiteral("action"), action);
    if (event != nullptr) {
        item.insert(QStringLiteral("eventId"), event->id);
        item.insert(QStringLiteral("stage"), static_cast<int>(event->stage));
        item.insert(QStringLiteral("dateKey"), event->dateKey);
        item.insert(QStringLiteral("completed"), event->completed);
    }
    if (!detail.isEmpty()) {
        item.insert(QStringLiteral("detail"), detail);
    }
    file.write(QJsonDocument(item).toJson(QJsonDocument::Compact));
    file.write("\n");
}

void NoteApp::appendSummaryHistory(const QString &requirement, const QString &notes, const QString &result) {
    QFile file(summary_history_path_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out << "## " << clock_->now().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n\n";
    out << "### 要求\n\n" << requirement << "\n\n";
    out << "### 结果\n\n" << result << "\n\n";
    out << "### 输入\n\n```text\n" << notes << "```\n\n";
}

void NoteApp::updateDateKey() {
    const QDate now = clock_->now().date();
    if (stage_ == NotebookStage::Day) {
        date_key_ = now.toString(QStringLiteral("yyyy-MM-dd"));
    } else if (stage_ == NotebookStage::Week) {
        int year = 0;
        const int week = now.weekNumber(&year);
        date_key_ = QStringLiteral("%1-W%2")
            .arg(year, 4, 10, QLatin1Char('0'))
            .arg(week, 2, 10, QLatin1Char('0'));
    } else if (stage_ == NotebookStage::Month) {
        date_key_ = now.toString(QStringLiteral("yyyy-MM"));
    } else {
        date_key_ = now.toString(QStringLiteral("yyyy"));
    }
}

void NoteApp::scheduleDateRefresh() {
    const QDateTime now = clock_->now();
    const QDateTime next_midnight(now.date().addDays(1), QTime(0, 0), now.timeZone());
    const qint64 delay = qBound<qint64>(1000, now.msecsTo(next_midnight) + 250, 24LL * 60 * 60 * 1000);
    date_refresh_timer_.start(static_cast<int>(delay));
}

void NoteApp::refreshDateIfNeeded() {
    const QString before = dateKey();
    updateDateKey();
    if (before == dateKey()) {
        return;
    }
    reload();
    emit stageChanged();
    setNotice(QStringLiteral("日期已更新到当前阶段。"));
}

void NoteApp::initPaths() {
    const LocalProfilePaths paths = LocalProfile::resolve();
    data_dir_ = paths.dataDir;
    notes_path_ = paths.legacyNotesPath;
    sqlite_path_ = paths.sqlitePath;
    config_path_ = paths.configPath;
    operation_log_path_ = paths.operationLogPath;
    summary_history_path_ = paths.summaryHistoryPath;
    backup_dir_ = paths.backupDir;
    credential_store_ = std::make_unique<CredentialStore>(data_dir_, paths.overridden);
}

void NoteApp::setNotice(const QString &value) {
    if (notice_ == value) return;
    notice_ = value;
    emit noticeChanged();
}

void NoteApp::rememberUndo(UndoType type, const NotebookNote &event, int derivedEventId) {
    undo_type_ = type;
    undo_event_ = event;
    undo_derived_event_id_ = derivedEventId;
    undo_expires_at_ = clock_->nowMSecsSinceEpoch() + 5000;
    emit undoChanged();
}

void NoteApp::clearUndo() {
    if (undo_type_ == UndoType::None && undo_expires_at_ == 0) {
        return;
    }
    undo_type_ = UndoType::None;
    undo_event_ = NotebookNote{};
    undo_derived_event_id_ = -1;
    undo_expires_at_ = 0;
    emit undoChanged();
}

void NoteApp::setAiState(const QString &state, const QString &error) {
    if (ai_state_ == state && ai_error_ == error) {
        return;
    }
    ai_state_ = state;
    ai_error_ = error;
    emit aiStateChanged();
}

AiClientRequest NoteApp::aiRequest(const QString &requirement,
                                   const QString &context) const {
    AiClientRequest request;
    request.endpoint = config_.apiUrl;
    request.model = config_.model;
    request.requirement = requirement;
    request.context = context;
    request.allowLocalHttp = config_.allowLocalHttp;
    if (credential_store_ != nullptr) {
        credential_store_->readSecret(&request.apiKey, nullptr);
    }
    return request;
}
