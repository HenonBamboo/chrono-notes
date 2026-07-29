#include "notebook.h"

#include "note_store.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <climits>
#include <utility>

namespace {

constexpr int kMaximumDateKeyLength = NOTE_KEY_MAX - 1;
constexpr int kMaximumSeriesIdLength = NOTE_SERIES_ID_MAX - 1;

NotebookError error(NotebookErrorCode code, const QString &message) {
    return NotebookError{code, message};
}

NotebookMutation failedMutation(NotebookErrorCode code, const QString &message) {
    NotebookMutation result;
    result.error = error(code, message);
    return result;
}

qint64 effectiveTimestamp(qint64 timestampSeconds) {
    return timestampSeconds > 0 ? timestampSeconds
                                : QDateTime::currentSecsSinceEpoch();
}

bool isValidStage(NotebookStage stage) {
    return stage >= NotebookStage::Day && stage <= NotebookStage::Year;
}

bool isValidRepeat(NotebookRepeat repeat) {
    return repeat >= NotebookRepeat::None && repeat <= NotebookRepeat::Yearly;
}

bool isValidCompletion(NotebookCompletion completion) {
    return completion >= NotebookCompletion::Any &&
           completion <= NotebookCompletion::Completed;
}

bool isValidBulkAction(NotebookBulkAction action) {
    return action >= NotebookBulkAction::ToggleCompletion &&
           action <= NotebookBulkAction::Remove;
}

QString repeatName(NotebookRepeat repeat) {
    switch (repeat) {
        case NotebookRepeat::None:
            return QStringLiteral("");
        case NotebookRepeat::Daily:
            return QStringLiteral("daily");
        case NotebookRepeat::Weekly:
            return QStringLiteral("weekly");
        case NotebookRepeat::Monthly:
            return QStringLiteral("monthly");
        case NotebookRepeat::Yearly:
            return QStringLiteral("yearly");
    }
    return QStringLiteral("");
}

std::optional<NotebookRepeat> repeatFromName(const QString &value) {
    if (value.isEmpty()) {
        return NotebookRepeat::None;
    }
    if (value == QStringLiteral("daily")) {
        return NotebookRepeat::Daily;
    }
    if (value == QStringLiteral("weekly")) {
        return NotebookRepeat::Weekly;
    }
    if (value == QStringLiteral("monthly")) {
        return NotebookRepeat::Monthly;
    }
    if (value == QStringLiteral("yearly")) {
        return NotebookRepeat::Yearly;
    }
    return std::nullopt;
}

bool isValidDateKey(NotebookStage stage, const QString &dateKey) {
    if (dateKey.isEmpty() || dateKey.size() > kMaximumDateKeyLength ||
        dateKey.contains(QChar::Null)) {
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
            if (!pattern.match(dateKey).hasMatch()) {
                return false;
            }
            const int year = dateKey.toInt();
            return year >= 1 && QDate(year, 1, 1).isValid();
        }
    }
    return false;
}

NotebookError validateText(const QString &text) {
    if (text.isEmpty()) {
        return error(NotebookErrorCode::InvalidArgument,
                     QStringLiteral("便签正文不能为空。"));
    }
    if (text.size() > Notebook::MaxTextLength) {
        return error(NotebookErrorCode::InvalidArgument,
                     QStringLiteral("便签正文不能超过 65,536 个字符。"));
    }
    if (text.contains(QChar::Null)) {
        return error(NotebookErrorCode::InvalidArgument,
                     QStringLiteral("便签正文不能包含空字符。"));
    }
    return {};
}

NotebookError validateIdentity(const NotebookNote &note, bool requireSeries) {
    if (note.id <= 0) {
        return error(NotebookErrorCode::InvalidArgument,
                     QStringLiteral("便签 ID 必须为正整数。"));
    }
    if (!isValidStage(note.stage) || !isValidDateKey(note.stage, note.dateKey)) {
        return error(NotebookErrorCode::InvalidArgument,
                     QStringLiteral("便签阶段或日期键无效。"));
    }
    if (const NotebookError textError = validateText(note.text); textError.isError()) {
        return textError;
    }
    if (!isValidRepeat(note.repeat) ||
        (note.stage != NotebookStage::Day && note.repeat != NotebookRepeat::None)) {
        return error(NotebookErrorCode::InvalidArgument,
                     QStringLiteral("重复规则无效；重复仅适用于每日便签。"));
    }
    if (note.completedAt < 0 || note.createdAt < 0 || note.updatedAt < 0 ||
        (!note.completed && note.completedAt != 0)) {
        return error(NotebookErrorCode::InvalidArgument,
                     QStringLiteral("便签时间或完成状态无效。"));
    }
    if ((requireSeries && note.seriesId.isEmpty()) ||
        note.seriesId.size() > kMaximumSeriesIdLength ||
        note.seriesId.contains(QChar::Null)) {
        return error(NotebookErrorCode::InvalidArgument,
                     QStringLiteral("便签系列 ID 缺失或过长。"));
    }
    return {};
}

QString nextRepeatDateKey(const NotebookNote &note) {
    if (note.stage != NotebookStage::Day || note.repeat == NotebookRepeat::None) {
        return {};
    }
    const QDate date = QDate::fromString(note.dateKey, QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid()) {
        return {};
    }

    QDate next;
    switch (note.repeat) {
        case NotebookRepeat::Daily:
            next = date.addDays(1);
            break;
        case NotebookRepeat::Weekly:
            next = date.addDays(7);
            break;
        case NotebookRepeat::Monthly:
            next = date.addMonths(1);
            break;
        case NotebookRepeat::Yearly:
            next = date.addYears(1);
            break;
        case NotebookRepeat::None:
            return {};
    }
    return next.isValid() ? next.toString(QStringLiteral("yyyy-MM-dd")) : QString();
}

qint64 activityTime(const NotebookNote &note) {
    return note.completed && note.completedAt > 0 ? note.completedAt : note.createdAt;
}

bool matchesQuery(const NotebookNote &note, const NotebookQuery &request) {
    if (request.stage.has_value() && note.stage != *request.stage) {
        return false;
    }
    if (!request.dateKey.isEmpty() && note.dateKey != request.dateKey) {
        return false;
    }
    if (request.completion == NotebookCompletion::Open && note.completed) {
        return false;
    }
    if (request.completion == NotebookCompletion::Completed && !note.completed) {
        return false;
    }
    return request.searchText.isEmpty() ||
           note.text.contains(request.searchText, Qt::CaseInsensitive);
}

QString sqliteError(const QString &operation, const QSqlError &sqlError) {
    const QString detail = sqlError.text().trimmed();
    return detail.isEmpty() ? operation : QStringLiteral("%1：%2").arg(operation, detail);
}

bool bindAndExecUpsert(QSqlDatabase &database,
                       const NotebookNote &note,
                       QString *failure) {
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO notes "
            "(id, stage, date_key, text, completed, completed_at, created_at, "
            "updated_at, repeat, series_id) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET "
            "stage=excluded.stage, date_key=excluded.date_key, text=excluded.text, "
            "completed=excluded.completed, completed_at=excluded.completed_at, "
            "created_at=excluded.created_at, updated_at=excluded.updated_at, "
            "repeat=excluded.repeat, series_id=excluded.series_id"))) {
        if (failure != nullptr) {
            *failure = sqliteError(QStringLiteral("无法准备便签写入"), query.lastError());
        }
        return false;
    }
    query.addBindValue(note.id);
    query.addBindValue(static_cast<int>(note.stage));
    query.addBindValue(note.dateKey);
    query.addBindValue(note.text);
    query.addBindValue(note.completed ? 1 : 0);
    query.addBindValue(note.completedAt);
    query.addBindValue(note.createdAt);
    query.addBindValue(note.updatedAt);
    query.addBindValue(repeatName(note.repeat));
    query.addBindValue(note.seriesId);
    if (!query.exec()) {
        if (failure != nullptr) {
            *failure = sqliteError(QStringLiteral("无法写入便签"), query.lastError());
        }
        return false;
    }
    return true;
}

bool deleteById(QSqlDatabase &database, int id, QString *failure) {
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral("DELETE FROM notes WHERE id=?"))) {
        if (failure != nullptr) {
            *failure = sqliteError(QStringLiteral("无法准备删除便签"), query.lastError());
        }
        return false;
    }
    query.addBindValue(id);
    if (!query.exec()) {
        if (failure != nullptr) {
            *failure = sqliteError(QStringLiteral("无法删除便签"), query.lastError());
        }
        return false;
    }
    return true;
}

void rollback(QSqlDatabase &database, QString *failure) {
    if (!database.rollback() && failure != nullptr) {
        *failure += QStringLiteral("；事务回滚失败：%1").arg(database.lastError().text());
    }
}

void closeConnection(QSqlDatabase *database, QString *connectionName) {
    if (database == nullptr || connectionName == nullptr || connectionName->isEmpty()) {
        return;
    }
    database->close();
    *database = QSqlDatabase();
    const QString name = std::exchange(*connectionName, QString());
    QSqlDatabase::removeDatabase(name);
}

}  // namespace

class Notebook::Impl {
public:
    ~Impl() {
        close();
    }

    void close() {
        closeConnection(&database, &connectionName);
        notes.clear();
        path.clear();
        nextId = 1;
    }

    bool opened() const {
        return database.isValid() && database.isOpen();
    }

    NotebookError notOpenError() const {
        return error(NotebookErrorCode::NotOpen,
                     QStringLiteral("便签数据库尚未打开。"));
    }

    NotebookError begin() {
        if (!opened()) {
            return notOpenError();
        }
        if (!database.transaction()) {
            return error(NotebookErrorCode::Persistence,
                         sqliteError(QStringLiteral("无法开始便签事务"),
                                     database.lastError()));
        }
        return {};
    }

    NotebookError commit() {
        if (!database.commit()) {
            QString failure =
                sqliteError(QStringLiteral("无法提交便签事务"), database.lastError());
            rollback(database, &failure);
            return error(NotebookErrorCode::Persistence, failure);
        }
        return {};
    }

    bool containsSeriesDate(const QString &seriesId,
                            const QString &dateKey,
                            const QHash<int, NotebookNote> &additional = {}) const {
        for (const NotebookNote &note : notes) {
            if (note.seriesId == seriesId && note.dateKey == dateKey) {
                return true;
            }
        }
        for (const NotebookNote &note : additional) {
            if (note.seriesId == seriesId && note.dateKey == dateKey) {
                return true;
            }
        }
        return false;
    }

    QString connectionName;
    QSqlDatabase database;
    QString path;
    QHash<int, NotebookNote> notes;
    int nextId{1};
};

Notebook::Notebook()
    : impl_(std::make_unique<Impl>()) {
}

Notebook::~Notebook() = default;

Notebook::Notebook(Notebook &&other) noexcept = default;

Notebook &Notebook::operator=(Notebook &&other) noexcept = default;

NotebookError Notebook::open(const QString &databasePath) {
    const QString resolvedPath =
        QFileInfo(databasePath.trimmed()).absoluteFilePath();
    if (databasePath.trimmed().isEmpty()) {
        return error(NotebookErrorCode::InvalidArgument,
                     QStringLiteral("便签数据库路径不能为空。"));
    }

    const QFileInfo info(resolvedPath);
    QDir parent = info.absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
        return error(NotebookErrorCode::Persistence,
                     QStringLiteral("无法创建便签数据目录：%1").arg(parent.absolutePath()));
    }

    const std::wstring widePath = resolvedPath.toStdWString();
    if (!note_store_prepare_sqlite(widePath.c_str())) {
        const QString detail = QString::fromWCharArray(note_store_last_error()).trimmed();
        NotebookErrorCode code = NotebookErrorCode::Persistence;
        if (detail.contains(QStringLiteral("newer than supported"),
                            Qt::CaseInsensitive)) {
            code = NotebookErrorCode::UnsupportedSchema;
        } else if (detail.contains(QStringLiteral("not a database"),
                                   Qt::CaseInsensitive) ||
                   detail.contains(QStringLiteral("malformed"),
                                   Qt::CaseInsensitive) ||
                   detail.contains(QStringLiteral("corrupt"),
                                   Qt::CaseInsensitive)) {
            code = NotebookErrorCode::CorruptData;
        }
        return error(code,
                     detail.isEmpty() ? QStringLiteral("无法准备便签数据库。") : detail);
    }

    QString candidateName =
        QStringLiteral("notebook_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QSqlDatabase candidate =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), candidateName);
    candidate.setDatabaseName(resolvedPath);
    if (!candidate.open()) {
        const QString message =
            sqliteError(QStringLiteral("无法打开便签数据库"), candidate.lastError());
        closeConnection(&candidate, &candidateName);
        return error(NotebookErrorCode::Persistence, message);
    }

    const QStringList pragmas = {
        QStringLiteral("PRAGMA busy_timeout=5000"),
        QStringLiteral("PRAGMA foreign_keys=ON"),
        QStringLiteral("PRAGMA journal_mode=WAL"),
        QStringLiteral("PRAGMA synchronous=NORMAL")
    };
    for (const QString &statement : pragmas) {
        QSqlQuery pragma(candidate);
        if (!pragma.exec(statement)) {
            const QString message =
                sqliteError(QStringLiteral("无法配置便签数据库"), pragma.lastError());
            closeConnection(&candidate, &candidateName);
            return error(NotebookErrorCode::Persistence, message);
        }
    }
#ifdef CHRONONOTES_TESTING
    bool hasTestPageLimit = false;
    const int testPageLimit =
        qEnvironmentVariableIntValue("CHRONONOTES_TEST_SQLITE_MAX_PAGE_COUNT",
                                     &hasTestPageLimit);
    if (hasTestPageLimit && testPageLimit > 0) {
        QSqlQuery pageLimit(candidate);
        if (!pageLimit.exec(
                QStringLiteral("PRAGMA max_page_count=%1").arg(testPageLimit)) ||
            !pageLimit.next() || pageLimit.value(0).toInt() != testPageLimit) {
            const QString message =
                sqliteError(QStringLiteral("无法设置测试数据库页上限"),
                            pageLimit.lastError());
            closeConnection(&candidate, &candidateName);
            return error(NotebookErrorCode::Persistence, message);
        }
    }
#endif

    QHash<int, NotebookNote> loaded;
    int candidateNextId = 1;
    QSqlQuery query(candidate);
    if (!query.exec(QStringLiteral(
            "SELECT id, stage, date_key, text, completed, completed_at, created_at, "
            "updated_at, repeat, series_id FROM notes ORDER BY id"))) {
        const QString message =
            sqliteError(QStringLiteral("无法读取便签数据库"), query.lastError());
        query = QSqlQuery();
        closeConnection(&candidate, &candidateName);
        return error(NotebookErrorCode::Persistence, message);
    }

    while (query.next()) {
        NotebookNote note;
        note.id = query.value(0).toInt();
        note.stage = static_cast<NotebookStage>(query.value(1).toInt());
        note.dateKey = query.value(2).toString();
        note.text = query.value(3).toString();
        const int completedValue = query.value(4).toInt();
        note.completed = completedValue != 0;
        note.completedAt = query.value(5).toLongLong();
        note.createdAt = query.value(6).toLongLong();
        note.updatedAt = query.value(7).toLongLong();
        const std::optional<NotebookRepeat> repeat =
            repeatFromName(query.value(8).toString());
        note.seriesId = query.value(9).toString();
        if (repeat.has_value()) {
            note.repeat = *repeat;
        }

        const NotebookError validation = validateIdentity(note, true);
        if (!repeat.has_value() || completedValue < 0 || completedValue > 1 ||
            validation.isError() || loaded.contains(note.id)) {
            const QString message = repeat.has_value() && !validation.isError()
                                        ? QStringLiteral("数据库包含重复或无效的便签 ID %1。")
                                              .arg(note.id)
                                        : (repeat.has_value()
                                               ? validation.message
                                               : QStringLiteral(
                                                     "数据库包含无效的重复规则。"));
            query.finish();
            query = QSqlQuery();
            closeConnection(&candidate, &candidateName);
            return error(NotebookErrorCode::CorruptData, message);
        }
        loaded.insert(note.id, note);
        if (note.id == INT_MAX) {
            query.finish();
            query = QSqlQuery();
            closeConnection(&candidate, &candidateName);
            return error(NotebookErrorCode::CorruptData,
                         QStringLiteral("便签 ID 已超出支持范围。"));
        }
        candidateNextId = std::max(candidateNextId, note.id + 1);
    }
    if (query.lastError().isValid()) {
        const QString message =
            sqliteError(QStringLiteral("读取便签数据库未完成"), query.lastError());
        query = QSqlQuery();
        closeConnection(&candidate, &candidateName);
        return error(NotebookErrorCode::Persistence, message);
    }
    query.finish();
    query = QSqlQuery();

    impl_->close();
    impl_->connectionName = candidateName;
    impl_->database = candidate;
    impl_->path = resolvedPath;
    impl_->notes = std::move(loaded);
    impl_->nextId = candidateNextId;
    return {};
}

void Notebook::close() {
    impl_->close();
}

bool Notebook::isOpen() const noexcept {
    return impl_->opened();
}

QString Notebook::databasePath() const {
    return impl_->path;
}

int Notebook::count() const noexcept {
    return impl_->notes.size();
}

NotebookOutcome<QList<NotebookNote>> Notebook::query(
    const NotebookQuery &request) const {
    NotebookOutcome<QList<NotebookNote>> result;
    if (!impl_->opened()) {
        result.error = impl_->notOpenError();
        return result;
    }
    if ((request.stage.has_value() && !isValidStage(*request.stage)) ||
        !isValidCompletion(request.completion) || request.limit < 0 ||
        request.dateKey.size() > kMaximumDateKeyLength ||
        request.dateKey.contains(QChar::Null) ||
        request.searchText.contains(QChar::Null)) {
        result.error = error(NotebookErrorCode::InvalidArgument,
                             QStringLiteral("便签查询条件无效。"));
        return result;
    }

    result.value.reserve(impl_->notes.size());
    for (const NotebookNote &note : impl_->notes) {
        if (matchesQuery(note, request)) {
            result.value.append(note);
        }
    }
    std::sort(result.value.begin(), result.value.end(),
              [](const NotebookNote &left, const NotebookNote &right) {
                  if (left.completed != right.completed) {
                      return !left.completed;
                  }
                  const qint64 leftActivity = activityTime(left);
                  const qint64 rightActivity = activityTime(right);
                  return leftActivity != rightActivity
                             ? leftActivity > rightActivity
                             : left.id > right.id;
              });
    if (request.limit > 0 && result.value.size() > request.limit) {
        result.value.erase(result.value.begin() + request.limit, result.value.end());
    }
    return result;
}

NotebookOutcome<NotebookNote> Notebook::note(int id) const {
    NotebookOutcome<NotebookNote> result;
    if (!impl_->opened()) {
        result.error = impl_->notOpenError();
        return result;
    }
    const auto found = impl_->notes.constFind(id);
    if (found == impl_->notes.cend()) {
        result.error = error(NotebookErrorCode::NotFound,
                             QStringLiteral("未找到指定便签。"));
        return result;
    }
    result.value = found.value();
    return result;
}

NotebookMutation Notebook::add(const NotebookDraft &draft,
                               qint64 timestampSeconds) {
    if (!impl_->opened()) {
        return failedMutation(NotebookErrorCode::NotOpen,
                              impl_->notOpenError().message);
    }
    if (!isValidStage(draft.stage) ||
        !isValidDateKey(draft.stage, draft.dateKey) ||
        !isValidRepeat(draft.repeat) ||
        (draft.stage != NotebookStage::Day && draft.repeat != NotebookRepeat::None)) {
        return failedMutation(NotebookErrorCode::InvalidArgument,
                              QStringLiteral("便签阶段、日期或重复规则无效。"));
    }
    if (const NotebookError textError = validateText(draft.text);
        textError.isError()) {
        return failedMutation(textError.code, textError.message);
    }
    if (draft.seriesId.size() > kMaximumSeriesIdLength ||
        draft.seriesId.contains(QChar::Null)) {
        return failedMutation(NotebookErrorCode::InvalidArgument,
                              QStringLiteral("便签系列 ID 过长或无效。"));
    }
    if (impl_->nextId <= 0 || impl_->nextId == INT_MAX) {
        return failedMutation(NotebookErrorCode::Conflict,
                              QStringLiteral("便签 ID 已耗尽。"));
    }

    NotebookNote created;
    created.id = impl_->nextId;
    created.stage = draft.stage;
    created.dateKey = draft.dateKey;
    created.text = draft.text;
    created.repeat = draft.repeat;
    created.seriesId =
        draft.seriesId.isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : draft.seriesId;
    created.createdAt = effectiveTimestamp(timestampSeconds);
    created.updatedAt = created.createdAt;

    if (const NotebookError transactionError = impl_->begin();
        transactionError.isError()) {
        return failedMutation(transactionError.code, transactionError.message);
    }
    QString failure;
    if (!bindAndExecUpsert(impl_->database, created, &failure)) {
        rollback(impl_->database, &failure);
        return failedMutation(NotebookErrorCode::Persistence, failure);
    }
    if (const NotebookError commitError = impl_->commit(); commitError.isError()) {
        return failedMutation(commitError.code, commitError.message);
    }

    impl_->notes.insert(created.id, created);
    ++impl_->nextId;

    NotebookMutation result;
    result.affected = 1;
    result.noteId = created.id;
    return result;
}

NotebookMutation Notebook::update(int id,
                                  const NotebookUpdate &updateValue,
                                  qint64 timestampSeconds) {
    if (!impl_->opened()) {
        return failedMutation(NotebookErrorCode::NotOpen,
                              impl_->notOpenError().message);
    }
    const auto found = impl_->notes.constFind(id);
    if (found == impl_->notes.cend()) {
        return failedMutation(NotebookErrorCode::NotFound,
                              QStringLiteral("未找到要修改的便签。"));
    }
    if (!updateValue.text.has_value() && !updateValue.repeat.has_value()) {
        return failedMutation(NotebookErrorCode::InvalidArgument,
                              QStringLiteral("便签修改内容不能为空。"));
    }

    NotebookNote changed = found.value();
    if (updateValue.text.has_value()) {
        if (const NotebookError textError = validateText(*updateValue.text);
            textError.isError()) {
            return failedMutation(textError.code, textError.message);
        }
        changed.text = *updateValue.text;
    }
    if (updateValue.repeat.has_value()) {
        if (!isValidRepeat(*updateValue.repeat) ||
            (changed.stage != NotebookStage::Day &&
             *updateValue.repeat != NotebookRepeat::None)) {
            return failedMutation(NotebookErrorCode::InvalidArgument,
                                  QStringLiteral("便签重复规则无效。"));
        }
        changed.repeat = *updateValue.repeat;
    }
    changed.updatedAt = effectiveTimestamp(timestampSeconds);

    if (const NotebookError transactionError = impl_->begin();
        transactionError.isError()) {
        return failedMutation(transactionError.code, transactionError.message);
    }
    QString failure;
    if (!bindAndExecUpsert(impl_->database, changed, &failure)) {
        rollback(impl_->database, &failure);
        return failedMutation(NotebookErrorCode::Persistence, failure);
    }
    if (const NotebookError commitError = impl_->commit(); commitError.isError()) {
        return failedMutation(commitError.code, commitError.message);
    }
    impl_->notes.insert(id, changed);

    NotebookMutation result;
    result.affected = 1;
    result.noteId = id;
    return result;
}

NotebookMutation Notebook::toggle(int id, qint64 timestampSeconds) {
    if (!impl_->opened()) {
        return failedMutation(NotebookErrorCode::NotOpen,
                              impl_->notOpenError().message);
    }
    const auto found = impl_->notes.constFind(id);
    if (found == impl_->notes.cend()) {
        return failedMutation(NotebookErrorCode::NotFound,
                              QStringLiteral("未找到要切换的便签。"));
    }

    const qint64 timestamp = effectiveTimestamp(timestampSeconds);
    NotebookNote changed = found.value();
    changed.completed = !changed.completed;
    changed.completedAt = changed.completed ? timestamp : 0;
    changed.updatedAt = timestamp;

    std::optional<NotebookNote> derived;
    int nextId = impl_->nextId;
    const QString nextDateKey = nextRepeatDateKey(changed);
    if (changed.completed && !nextDateKey.isEmpty() &&
        !impl_->containsSeriesDate(changed.seriesId, nextDateKey)) {
        if (nextId <= 0 || nextId == INT_MAX) {
            return failedMutation(NotebookErrorCode::Conflict,
                                  QStringLiteral("便签 ID 已耗尽。"));
        }
        NotebookNote next;
        next.id = nextId;
        next.stage = NotebookStage::Day;
        next.dateKey = nextDateKey;
        next.text = changed.text;
        next.repeat = changed.repeat;
        next.seriesId = changed.seriesId;
        next.createdAt = timestamp;
        next.updatedAt = timestamp;
        derived = next;
        ++nextId;
    }

    if (const NotebookError transactionError = impl_->begin();
        transactionError.isError()) {
        return failedMutation(transactionError.code, transactionError.message);
    }
    QString failure;
    if (!bindAndExecUpsert(impl_->database, changed, &failure) ||
        (derived.has_value() &&
         !bindAndExecUpsert(impl_->database, *derived, &failure))) {
        rollback(impl_->database, &failure);
        return failedMutation(NotebookErrorCode::Persistence, failure);
    }
    if (const NotebookError commitError = impl_->commit(); commitError.isError()) {
        return failedMutation(commitError.code, commitError.message);
    }

    impl_->notes.insert(id, changed);
    NotebookMutation result;
    result.affected = 1;
    result.noteId = id;
    if (derived.has_value()) {
        impl_->notes.insert(derived->id, *derived);
        impl_->nextId = nextId;
        result.derivedNoteIds.append(derived->id);
    }
    return result;
}

NotebookMutation Notebook::remove(int id) {
    if (!impl_->opened()) {
        return failedMutation(NotebookErrorCode::NotOpen,
                              impl_->notOpenError().message);
    }
    if (!impl_->notes.contains(id)) {
        return failedMutation(NotebookErrorCode::NotFound,
                              QStringLiteral("未找到要删除的便签。"));
    }
    if (const NotebookError transactionError = impl_->begin();
        transactionError.isError()) {
        return failedMutation(transactionError.code, transactionError.message);
    }
    QString failure;
    if (!deleteById(impl_->database, id, &failure)) {
        rollback(impl_->database, &failure);
        return failedMutation(NotebookErrorCode::Persistence, failure);
    }
    if (const NotebookError commitError = impl_->commit(); commitError.isError()) {
        return failedMutation(commitError.code, commitError.message);
    }
    impl_->notes.remove(id);

    NotebookMutation result;
    result.affected = 1;
    result.noteId = id;
    return result;
}

NotebookMutation Notebook::bulk(NotebookBulkAction action,
                                const NotebookQuery &request,
                                qint64 timestampSeconds) {
    if (!impl_->opened()) {
        return failedMutation(NotebookErrorCode::NotOpen,
                              impl_->notOpenError().message);
    }
    if (!isValidBulkAction(action) ||
        (request.stage.has_value() && !isValidStage(*request.stage)) ||
        !isValidCompletion(request.completion) || request.limit < 0 ||
        request.dateKey.size() > kMaximumDateKeyLength ||
        request.dateKey.contains(QChar::Null) ||
        request.searchText.contains(QChar::Null)) {
        return failedMutation(NotebookErrorCode::InvalidArgument,
                              QStringLiteral("批量操作或查询条件无效。"));
    }

    QList<NotebookNote> candidates;
    candidates.reserve(impl_->notes.size());
    for (const NotebookNote &note : impl_->notes) {
        if (matchesQuery(note, request)) {
            candidates.append(note);
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const NotebookNote &left, const NotebookNote &right) {
                  if (left.completed != right.completed) {
                      return !left.completed;
                  }
                  const qint64 leftActivity = activityTime(left);
                  const qint64 rightActivity = activityTime(right);
                  return leftActivity != rightActivity
                             ? leftActivity > rightActivity
                             : left.id > right.id;
              });
    if (request.limit > 0 && candidates.size() > request.limit) {
        candidates.erase(candidates.begin() + request.limit, candidates.end());
    }
    QList<int> ids;
    ids.reserve(candidates.size());
    for (const NotebookNote &note : std::as_const(candidates)) {
        ids.append(note.id);
    }
    if (ids.isEmpty()) {
        NotebookMutation result;
        return result;
    }

    bool targetCompleted = false;
    if (action == NotebookBulkAction::ToggleCompletion) {
        targetCompleted =
            std::any_of(ids.cbegin(), ids.cend(), [this](int id) {
                return !impl_->notes.value(id).completed;
            });
    } else if (action == NotebookBulkAction::Complete) {
        targetCompleted = true;
    }

    const qint64 timestamp = effectiveTimestamp(timestampSeconds);
    QHash<int, NotebookNote> changed;
    QHash<int, NotebookNote> derived;
    int nextId = impl_->nextId;
    if (action != NotebookBulkAction::Remove) {
        for (const int id : std::as_const(ids)) {
            NotebookNote note = impl_->notes.value(id);
            if (note.completed == targetCompleted) {
                continue;
            }
            note.completed = targetCompleted;
            note.completedAt = targetCompleted ? timestamp : 0;
            note.updatedAt = timestamp;
            changed.insert(id, note);

            const QString nextDateKey = nextRepeatDateKey(note);
            if (targetCompleted && !nextDateKey.isEmpty() &&
                !impl_->containsSeriesDate(note.seriesId, nextDateKey, derived)) {
                if (nextId <= 0 || nextId == INT_MAX) {
                    return failedMutation(NotebookErrorCode::Conflict,
                                          QStringLiteral("便签 ID 已耗尽。"));
                }
                NotebookNote next;
                next.id = nextId++;
                next.stage = NotebookStage::Day;
                next.dateKey = nextDateKey;
                next.text = note.text;
                next.repeat = note.repeat;
                next.seriesId = note.seriesId;
                next.createdAt = timestamp;
                next.updatedAt = timestamp;
                derived.insert(next.id, next);
            }
        }
        if (changed.isEmpty()) {
            NotebookMutation result;
            return result;
        }
    }

    if (const NotebookError transactionError = impl_->begin();
        transactionError.isError()) {
        return failedMutation(transactionError.code, transactionError.message);
    }
    QString failure;
    bool persisted = true;
    if (action == NotebookBulkAction::Remove) {
        for (const int id : std::as_const(ids)) {
            if (!deleteById(impl_->database, id, &failure)) {
                persisted = false;
                break;
            }
        }
    } else {
        for (const NotebookNote &note : changed) {
            if (!bindAndExecUpsert(impl_->database, note, &failure)) {
                persisted = false;
                break;
            }
        }
        if (persisted) {
            for (const NotebookNote &note : derived) {
                if (!bindAndExecUpsert(impl_->database, note, &failure)) {
                    persisted = false;
                    break;
                }
            }
        }
    }
    if (!persisted) {
        rollback(impl_->database, &failure);
        return failedMutation(NotebookErrorCode::Persistence, failure);
    }
    if (const NotebookError commitError = impl_->commit(); commitError.isError()) {
        return failedMutation(commitError.code, commitError.message);
    }

    NotebookMutation result;
    if (action == NotebookBulkAction::Remove) {
        for (const int id : std::as_const(ids)) {
            impl_->notes.remove(id);
        }
        result.affected = ids.size();
    } else {
        for (auto it = changed.cbegin(); it != changed.cend(); ++it) {
            impl_->notes.insert(it.key(), it.value());
        }
        for (auto it = derived.cbegin(); it != derived.cend(); ++it) {
            impl_->notes.insert(it.key(), it.value());
            result.derivedNoteIds.append(it.key());
        }
        impl_->nextId = nextId;
        result.affected = changed.size();
        std::sort(result.derivedNoteIds.begin(), result.derivedNoteIds.end());
    }
    return result;
}

NotebookMutation Notebook::restore(const NotebookNote &noteValue) {
    if (!impl_->opened()) {
        return failedMutation(NotebookErrorCode::NotOpen,
                              impl_->notOpenError().message);
    }
    if (impl_->notes.contains(noteValue.id)) {
        return failedMutation(NotebookErrorCode::Conflict,
                              QStringLiteral("相同 ID 的便签已经存在。"));
    }
    if (const NotebookError validation = validateIdentity(noteValue, true);
        validation.isError()) {
        return failedMutation(validation.code, validation.message);
    }
    if (noteValue.id == INT_MAX) {
        return failedMutation(NotebookErrorCode::Conflict,
                              QStringLiteral("便签 ID 已超出支持范围。"));
    }

    if (const NotebookError transactionError = impl_->begin();
        transactionError.isError()) {
        return failedMutation(transactionError.code, transactionError.message);
    }
    QString failure;
    if (!bindAndExecUpsert(impl_->database, noteValue, &failure)) {
        rollback(impl_->database, &failure);
        return failedMutation(NotebookErrorCode::Persistence, failure);
    }
    if (const NotebookError commitError = impl_->commit(); commitError.isError()) {
        return failedMutation(commitError.code, commitError.message);
    }

    impl_->notes.insert(noteValue.id, noteValue);
    impl_->nextId = std::max(impl_->nextId, noteValue.id + 1);
    NotebookMutation result;
    result.affected = 1;
    result.noteId = noteValue.id;
    return result;
}

NotebookMutation Notebook::restoreReplacing(const NotebookNote &snapshotValue,
                                            const QList<int> &removeIds) {
    if (!impl_->opened()) {
        return failedMutation(NotebookErrorCode::NotOpen,
                              impl_->notOpenError().message);
    }
    if (const NotebookError validation = validateIdentity(snapshotValue, true);
        validation.isError()) {
        return failedMutation(validation.code, validation.message);
    }
    if (snapshotValue.id == INT_MAX) {
        return failedMutation(NotebookErrorCode::Conflict,
                              QStringLiteral("便签 ID 已超出支持范围。"));
    }

    QSet<int> uniqueRemoveIds;
    for (const int id : removeIds) {
        if (id <= 0 || id == snapshotValue.id) {
            return failedMutation(
                NotebookErrorCode::InvalidArgument,
                QStringLiteral("撤销派生便签 ID 无效或与原便签冲突。"));
        }
        uniqueRemoveIds.insert(id);
    }

    if (const NotebookError transactionError = impl_->begin();
        transactionError.isError()) {
        return failedMutation(transactionError.code, transactionError.message);
    }
    QString failure;
    bool persisted = bindAndExecUpsert(impl_->database, snapshotValue, &failure);
    if (persisted) {
        for (const int id : std::as_const(uniqueRemoveIds)) {
            if (!deleteById(impl_->database, id, &failure)) {
                persisted = false;
                break;
            }
        }
    }
    if (!persisted) {
        rollback(impl_->database, &failure);
        return failedMutation(NotebookErrorCode::Persistence, failure);
    }
    if (const NotebookError commitError = impl_->commit(); commitError.isError()) {
        return failedMutation(commitError.code, commitError.message);
    }

    int removed = 0;
    for (const int id : std::as_const(uniqueRemoveIds)) {
        removed += impl_->notes.remove(id);
    }
    impl_->notes.insert(snapshotValue.id, snapshotValue);
    impl_->nextId = std::max(impl_->nextId, snapshotValue.id + 1);

    NotebookMutation result;
    result.affected = 1 + removed;
    result.noteId = snapshotValue.id;
    return result;
}

NotebookOutcome<NotebookSnapshot> Notebook::snapshot() const {
    NotebookOutcome<NotebookSnapshot> result;
    if (!impl_->opened()) {
        result.error = impl_->notOpenError();
        return result;
    }
    result.value.schemaVersion = CurrentSchemaVersion;
    result.value.notes.reserve(impl_->notes.size());
    for (const NotebookNote &note : impl_->notes) {
        result.value.notes.append(note);
    }
    std::sort(result.value.notes.begin(), result.value.notes.end(),
              [](const NotebookNote &left, const NotebookNote &right) {
                  return left.id < right.id;
              });
    return result;
}

NotebookMutation Notebook::importSnapshot(const NotebookSnapshot &snapshotValue) {
    if (!impl_->opened()) {
        return failedMutation(NotebookErrorCode::NotOpen,
                              impl_->notOpenError().message);
    }
    if (snapshotValue.schemaVersion != 1 &&
        snapshotValue.schemaVersion != CurrentSchemaVersion) {
        return failedMutation(
            NotebookErrorCode::UnsupportedSchema,
            QStringLiteral("不支持工作区便签 schema %1。")
                .arg(snapshotValue.schemaVersion));
    }

    QHash<int, NotebookNote> imported;
    int nextId = 1;
    for (NotebookNote note : snapshotValue.notes) {
        if (snapshotValue.schemaVersion == 1 && note.seriesId.isEmpty()) {
            note.seriesId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        if (const NotebookError validation = validateIdentity(note, true);
            validation.isError()) {
            return failedMutation(validation.code, validation.message);
        }
        if (imported.contains(note.id)) {
            return failedMutation(NotebookErrorCode::Conflict,
                                  QStringLiteral("导入包含重复便签 ID %1。").arg(note.id));
        }
        if (note.id == INT_MAX) {
            return failedMutation(NotebookErrorCode::Conflict,
                                  QStringLiteral("导入便签 ID 已超出支持范围。"));
        }
        imported.insert(note.id, note);
        nextId = std::max(nextId, note.id + 1);
    }

    if (const NotebookError transactionError = impl_->begin();
        transactionError.isError()) {
        return failedMutation(transactionError.code, transactionError.message);
    }
    QString failure;
    bool persisted = true;
    for (const NotebookNote &note : imported) {
        if (!bindAndExecUpsert(impl_->database, note, &failure)) {
            persisted = false;
            break;
        }
    }
    if (persisted) {
        QSqlQuery removeMissing(impl_->database);
        if (!removeMissing.exec(QStringLiteral(
                "CREATE TEMP TABLE IF NOT EXISTS notebook_import_ids "
                "(id INTEGER PRIMARY KEY)")) ||
            !removeMissing.exec(QStringLiteral("DELETE FROM notebook_import_ids"))) {
            failure =
                sqliteError(QStringLiteral("无法准备导入身份集合"),
                            removeMissing.lastError());
            persisted = false;
        }
        if (persisted) {
            QSqlQuery remember(impl_->database);
            if (!remember.prepare(
                    QStringLiteral("INSERT INTO notebook_import_ids(id) VALUES (?)"))) {
                failure = sqliteError(QStringLiteral("无法准备导入身份"),
                                      remember.lastError());
                persisted = false;
            } else {
                for (const int id : imported.keys()) {
                    remember.bindValue(0, id);
                    if (!remember.exec()) {
                        failure = sqliteError(QStringLiteral("无法记录导入身份"),
                                              remember.lastError());
                        persisted = false;
                        break;
                    }
                }
            }
        }
        if (persisted &&
            !removeMissing.exec(QStringLiteral(
                "DELETE FROM notes WHERE NOT EXISTS "
                "(SELECT 1 FROM notebook_import_ids "
                "WHERE notebook_import_ids.id=notes.id)"))) {
            failure = sqliteError(QStringLiteral("无法移除导入中不存在的便签"),
                                  removeMissing.lastError());
            persisted = false;
        }
    }
    if (!persisted) {
        rollback(impl_->database, &failure);
        return failedMutation(NotebookErrorCode::Persistence, failure);
    }
    if (const NotebookError commitError = impl_->commit(); commitError.isError()) {
        return failedMutation(commitError.code, commitError.message);
    }

    impl_->notes = std::move(imported);
    impl_->nextId = nextId;
    NotebookMutation result;
    result.affected = impl_->notes.size();
    return result;
}

NotebookMutation Notebook::importLegacyFile(const QString &legacyPath) {
    if (!impl_->opened()) {
        return failedMutation(NotebookErrorCode::NotOpen,
                              impl_->notOpenError().message);
    }
    const QString trimmedPath = legacyPath.trimmed();
    if (trimmedPath.isEmpty()) {
        return failedMutation(NotebookErrorCode::InvalidArgument,
                              QStringLiteral("旧便签文件路径不能为空。"));
    }
    const QFileInfo info(trimmedPath);
    if (!info.exists() || !info.isFile()) {
        return failedMutation(NotebookErrorCode::NotFound,
                              QStringLiteral("未找到旧便签文件。"));
    }

    NoteStore legacy;
    note_store_init(&legacy);
    const std::wstring widePath = info.absoluteFilePath().toStdWString();
    if (!note_store_load(&legacy, widePath.c_str())) {
        const QString detail = QString::fromWCharArray(note_store_last_error()).trimmed();
        const NotebookErrorCode code =
            detail.startsWith(QStringLiteral("Cannot open"), Qt::CaseInsensitive) ||
                    detail.startsWith(QStringLiteral("Cannot read"), Qt::CaseInsensitive)
                ? NotebookErrorCode::Persistence
                : NotebookErrorCode::CorruptData;
        return failedMutation(
            code,
            detail.isEmpty() ? QStringLiteral("旧便签文件无法读取。") : detail);
    }

    NotebookSnapshot converted;
    converted.schemaVersion = 1;
    converted.notes.reserve(legacy.count);
    for (const NoteEvent &event : legacy.items) {
        const std::optional<NotebookRepeat> repeat =
            repeatFromName(QString::fromWCharArray(event.repeat));
        if (!repeat.has_value()) {
            return failedMutation(NotebookErrorCode::CorruptData,
                                  QStringLiteral("旧便签包含无效的重复规则。"));
        }

        NotebookNote note;
        note.id = event.id;
        note.stage = static_cast<NotebookStage>(event.stage);
        note.dateKey = QString::fromWCharArray(event.date_key);
        note.text = QString::fromWCharArray(event.text);
        note.completed = event.completed != 0;
        note.completedAt = event.completed_at;
        note.createdAt = event.created_at;
        note.updatedAt = event.updated_at;
        note.repeat = *repeat;
        note.seriesId = QString::fromWCharArray(event.series_id);
        converted.notes.append(std::move(note));
    }
    return importSnapshot(converted);
}
