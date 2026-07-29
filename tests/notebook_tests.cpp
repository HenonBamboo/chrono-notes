#include "notebook.h"

#include <QElapsedTimer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString databasePath(const QTemporaryDir &directory) {
    return directory.filePath(QStringLiteral("notes.sqlite"));
}

bool executeSql(const QString &path, const QString &statement) {
    const QString connectionName =
        QStringLiteral("notebook_test_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    bool ok = false;
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            ok = query.exec(statement);
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}

NotebookDraft dailyDraft(const QString &text,
                         const QString &dateKey = QStringLiteral("2026-07-26")) {
    NotebookDraft draft;
    draft.stage = NotebookStage::Day;
    draft.dateKey = dateKey;
    draft.text = text;
    return draft;
}

NotebookNote importedNote(int id, const QString &text) {
    NotebookNote note;
    note.id = id;
    note.stage = NotebookStage::Day;
    note.dateKey = QStringLiteral("2026-07-26");
    note.text = text;
    note.createdAt = 1'000 + id;
    note.updatedAt = note.createdAt;
    note.seriesId = QStringLiteral("series-%1").arg(id);
    return note;
}

}  // namespace

class NotebookTests : public QObject {
    Q_OBJECT

private slots:
    void openCrudAndReopen();
    void incrementalUpdateDoesNotRewriteOtherRows();
    void failedMutationDoesNotChangeVisibleState();
    void failedOpenPreservesCurrentNotebook();
    void textLimitIsExactAndNeverTruncates();
    void recurringCompletionIsOneStableSeriesTransaction();
    void recurringUndoRestoresAndRemovesDerivedAtomically();
    void bulkRestoreSnapshotAndLegacyImport();
    void legacyFileImportStaysBehindNotebookInterface();
    void queryTenThousandNotesUnderThreshold();
    void lockedDatabaseRejectsWriteWithoutChangingState();
    void readOnlyDatabaseOpenPreservesCurrentNotebook();
    void corruptDatabaseOpenPreservesCurrentNotebook();
    void diskFullRollsBackFailedWrite();
    void failedMigrationKeepsSourceAndCreatesBackup();
    void persistenceP95StaysUnderThreshold();
};

void NotebookTests::openCrudAndReopen() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = databasePath(directory);

    Notebook notebook;
    QVERIFY2(!notebook.query().ok(), "query before open must report NotOpen");
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    QVERIFY(notebook.isOpen());
    QCOMPARE(notebook.databasePath(), QFileInfo(path).absoluteFilePath());

    NotebookMutation add = notebook.add(dailyDraft(QStringLiteral("第一条")), 100);
    QVERIFY2(add.ok(), qPrintable(add.error.message));
    QCOMPARE(add.affected, 1);
    QCOMPARE(add.noteId, 1);

    NotebookUpdate update;
    update.text = QStringLiteral("修改后");
    update.repeat = NotebookRepeat::Weekly;
    NotebookMutation changed = notebook.update(add.noteId, update, 200);
    QVERIFY2(changed.ok(), qPrintable(changed.error.message));

    NotebookOutcome<NotebookNote> stored = notebook.note(add.noteId);
    QVERIFY(stored.ok());
    QCOMPARE(stored.value.text, QStringLiteral("修改后"));
    QCOMPARE(stored.value.repeat, NotebookRepeat::Weekly);
    QCOMPARE(stored.value.updatedAt, 200);

    NotebookQuery query;
    query.stage = NotebookStage::Day;
    query.dateKey = QStringLiteral("2026-07-26");
    query.searchText = QStringLiteral("修改");
    const NotebookOutcome<QList<NotebookNote>> queried = notebook.query(query);
    QVERIFY(queried.ok());
    QCOMPARE(queried.value.size(), 1);

    const NotebookNote deletedSnapshot = stored.value;
    QVERIFY(notebook.remove(add.noteId).ok());
    QCOMPARE(notebook.count(), 0);
    QVERIFY(notebook.restore(deletedSnapshot).ok());
    QCOMPARE(notebook.count(), 1);

    notebook.close();
    QVERIFY(!notebook.isOpen());
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    const NotebookOutcome<NotebookNote> reopened = notebook.note(add.noteId);
    QVERIFY(reopened.ok());
    QVERIFY(reopened.value == deletedSnapshot);
}

void NotebookTests::failedMutationDoesNotChangeVisibleState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = databasePath(directory);

    Notebook notebook;
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    const NotebookMutation added =
        notebook.add(dailyDraft(QStringLiteral("保持原样")), 100);
    QVERIFY(added.ok());

    QVERIFY(executeSql(
        path,
        QStringLiteral("CREATE TRIGGER reject_updates BEFORE UPDATE ON notes "
                       "BEGIN SELECT RAISE(ABORT, 'blocked update'); END")));

    NotebookUpdate update;
    update.text = QStringLiteral("不应可见");
    const NotebookMutation failed = notebook.update(added.noteId, update, 200);
    QVERIFY(!failed.ok());
    QCOMPARE(failed.error.code, NotebookErrorCode::Persistence);

    const NotebookOutcome<NotebookNote> visible = notebook.note(added.noteId);
    QVERIFY(visible.ok());
    QCOMPARE(visible.value.text, QStringLiteral("保持原样"));
    QCOMPARE(visible.value.updatedAt, 100);

    notebook.close();
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    QCOMPARE(notebook.note(added.noteId).value.text, QStringLiteral("保持原样"));
}

void NotebookTests::incrementalUpdateDoesNotRewriteOtherRows() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = databasePath(directory);

    Notebook notebook;
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    const NotebookMutation first =
        notebook.add(dailyDraft(QStringLiteral("第一条")), 100);
    const NotebookMutation second =
        notebook.add(dailyDraft(QStringLiteral("第二条")), 101);
    QVERIFY(first.ok());
    QVERIFY(second.ok());
    QVERIFY(executeSql(
        path,
        QStringLiteral("CREATE TRIGGER reject_deletes BEFORE DELETE ON notes "
                       "BEGIN SELECT RAISE(ABORT, 'unexpected delete'); END")));

    NotebookUpdate update;
    update.text = QStringLiteral("只更新第一条");
    const NotebookMutation changed = notebook.update(first.noteId, update, 200);
    QVERIFY2(changed.ok(), qPrintable(changed.error.message));
    QCOMPARE(notebook.note(first.noteId).value.text, QStringLiteral("只更新第一条"));
    QCOMPARE(notebook.note(second.noteId).value.text, QStringLiteral("第二条"));

    const NotebookMutation rejectedDelete = notebook.remove(first.noteId);
    QVERIFY(!rejectedDelete.ok());
    QCOMPARE(rejectedDelete.error.code, NotebookErrorCode::Persistence);
    QCOMPARE(notebook.count(), 2);
}

void NotebookTests::failedOpenPreservesCurrentNotebook() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString currentPath = directory.filePath(QStringLiteral("current.sqlite"));
    const QString futurePath = directory.filePath(QStringLiteral("future.sqlite"));

    Notebook notebook;
    QVERIFY(notebook.open(currentPath).code == NotebookErrorCode::None);
    const NotebookMutation added =
        notebook.add(dailyDraft(QStringLiteral("仍可使用")), 100);
    QVERIFY(added.ok());

    QVERIFY(executeSql(futurePath, QStringLiteral("PRAGMA user_version=99")));
    const NotebookError openError = notebook.open(futurePath);
    QCOMPARE(openError.code, NotebookErrorCode::UnsupportedSchema);
    QVERIFY(notebook.isOpen());
    QCOMPARE(notebook.databasePath(), QFileInfo(currentPath).absoluteFilePath());
    QCOMPARE(notebook.note(added.noteId).value.text, QStringLiteral("仍可使用"));
}

void NotebookTests::textLimitIsExactAndNeverTruncates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    Notebook notebook;
    QVERIFY(notebook.open(databasePath(directory)).code == NotebookErrorCode::None);
    const QString maximum(Notebook::MaxTextLength, QLatin1Char('x'));
    const NotebookMutation accepted = notebook.add(dailyDraft(maximum), 100);
    QVERIFY2(accepted.ok(), qPrintable(accepted.error.message));
    QCOMPARE(notebook.note(accepted.noteId).value.text.size(),
             Notebook::MaxTextLength);

    const int beforeCount = notebook.count();
    const QString oversized(Notebook::MaxTextLength + 1, QLatin1Char('y'));
    const NotebookMutation rejected = notebook.add(dailyDraft(oversized), 101);
    QVERIFY(!rejected.ok());
    QCOMPARE(rejected.error.code, NotebookErrorCode::InvalidArgument);
    QCOMPARE(notebook.count(), beforeCount);

    NotebookUpdate update;
    update.text = oversized;
    const NotebookMutation rejectedUpdate =
        notebook.update(accepted.noteId, update, 102);
    QVERIFY(!rejectedUpdate.ok());
    QCOMPARE(notebook.note(accepted.noteId).value.text, maximum);
}

void NotebookTests::recurringCompletionIsOneStableSeriesTransaction() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = databasePath(directory);

    Notebook notebook;
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    NotebookDraft draft = dailyDraft(QStringLiteral("月底复盘"),
                                     QStringLiteral("2028-01-31"));
    draft.repeat = NotebookRepeat::Monthly;
    const NotebookMutation added = notebook.add(draft, 100);
    QVERIFY(added.ok());
    const QString series = notebook.note(added.noteId).value.seriesId;
    QVERIFY(!series.isEmpty());

    const NotebookMutation toggled = notebook.toggle(added.noteId, 200);
    QVERIFY2(toggled.ok(), qPrintable(toggled.error.message));
    QCOMPARE(toggled.derivedNoteIds.size(), 1);
    QCOMPARE(notebook.count(), 2);

    const NotebookNote completed = notebook.note(added.noteId).value;
    const NotebookNote next = notebook.note(toggled.derivedNoteIds.first()).value;
    QVERIFY(completed.completed);
    QCOMPARE(completed.completedAt, 200);
    QCOMPARE(next.dateKey, QStringLiteral("2028-02-29"));
    QCOMPARE(next.seriesId, series);
    QCOMPARE(next.repeat, NotebookRepeat::Monthly);
    QVERIFY(!next.completed);

    notebook.close();
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    QCOMPARE(notebook.count(), 2);
    QCOMPARE(notebook.note(toggled.derivedNoteIds.first()).value.seriesId, series);
}

void NotebookTests::recurringUndoRestoresAndRemovesDerivedAtomically() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = databasePath(directory);

    Notebook notebook;
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    NotebookDraft draft =
        dailyDraft(QStringLiteral("每日复盘"), QStringLiteral("2026-07-26"));
    draft.repeat = NotebookRepeat::Daily;
    const NotebookMutation added = notebook.add(draft, 100);
    QVERIFY(added.ok());
    const NotebookNote before = notebook.note(added.noteId).value;

    const NotebookMutation toggled = notebook.toggle(added.noteId, 200);
    QVERIFY(toggled.ok());
    QCOMPARE(toggled.derivedNoteIds.size(), 1);
    const int derivedId = toggled.derivedNoteIds.first();
    QVERIFY(notebook.note(added.noteId).value.completed);
    QVERIFY(notebook.note(derivedId).ok());

    const NotebookMutation undone =
        notebook.restoreReplacing(before, toggled.derivedNoteIds);
    QVERIFY2(undone.ok(), qPrintable(undone.error.message));
    QCOMPARE(undone.affected, 2);
    QVERIFY(!notebook.note(added.noteId).value.completed);
    QVERIFY(!notebook.note(derivedId).ok());

    notebook.close();
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    QVERIFY(!notebook.note(added.noteId).value.completed);
    QVERIFY(!notebook.note(derivedId).ok());

    const NotebookMutation toggledAgain = notebook.toggle(added.noteId, 300);
    QVERIFY(toggledAgain.ok());
    QCOMPARE(toggledAgain.derivedNoteIds.size(), 1);
    const int secondDerivedId = toggledAgain.derivedNoteIds.first();
    QVERIFY(executeSql(
        path,
        QStringLiteral("CREATE TRIGGER reject_undo_delete BEFORE DELETE ON notes "
                       "BEGIN SELECT RAISE(ABORT, 'blocked undo delete'); END")));
    const NotebookMutation rejectedUndo =
        notebook.restoreReplacing(before, toggledAgain.derivedNoteIds);
    QVERIFY(!rejectedUndo.ok());
    QCOMPARE(rejectedUndo.error.code, NotebookErrorCode::Persistence);
    QVERIFY(notebook.note(added.noteId).value.completed);
    QVERIFY(notebook.note(secondDerivedId).ok());

    notebook.close();
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    QVERIFY(notebook.note(added.noteId).value.completed);
    QVERIFY(notebook.note(secondDerivedId).ok());
}

void NotebookTests::bulkRestoreSnapshotAndLegacyImport() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = databasePath(directory);

    Notebook notebook;
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    QVERIFY(notebook.add(dailyDraft(QStringLiteral("甲")), 100).ok());
    QVERIFY(notebook.add(dailyDraft(QStringLiteral("乙")), 101).ok());

    NotebookQuery currentDay;
    currentDay.stage = NotebookStage::Day;
    currentDay.dateKey = QStringLiteral("2026-07-26");
    const NotebookMutation completed =
        notebook.bulk(NotebookBulkAction::Complete, currentDay, 200);
    QVERIFY(completed.ok());
    QCOMPARE(completed.affected, 2);

    NotebookQuery completedOnly = currentDay;
    completedOnly.completion = NotebookCompletion::Completed;
    QCOMPARE(notebook.query(completedOnly).value.size(), 2);

    const NotebookOutcome<NotebookSnapshot> snapshot = notebook.snapshot();
    QVERIFY(snapshot.ok());
    QCOMPARE(snapshot.value.schemaVersion, Notebook::CurrentSchemaVersion);

    const NotebookMutation removed =
        notebook.bulk(NotebookBulkAction::Remove, completedOnly);
    QVERIFY(removed.ok());
    QCOMPARE(removed.affected, 2);
    QCOMPARE(notebook.count(), 0);

    QVERIFY(notebook.importSnapshot(snapshot.value).ok());
    QCOMPARE(notebook.count(), 2);

    NotebookSnapshot legacy;
    legacy.schemaVersion = 1;
    NotebookNote old = importedNote(9, QStringLiteral("旧 JSON 便签"));
    old.seriesId.clear();
    legacy.notes.append(old);
    const NotebookMutation imported = notebook.importSnapshot(legacy);
    QVERIFY2(imported.ok(), qPrintable(imported.error.message));
    QCOMPARE(notebook.count(), 1);
    QVERIFY(!notebook.note(9).value.seriesId.isEmpty());

    NotebookSnapshot duplicate = legacy;
    duplicate.notes.append(old);
    const NotebookMutation rejected = notebook.importSnapshot(duplicate);
    QVERIFY(!rejected.ok());
    QCOMPARE(notebook.count(), 1);
    QCOMPARE(notebook.note(9).value.text, QStringLiteral("旧 JSON 便签"));
}

void NotebookTests::legacyFileImportStaysBehindNotebookInterface() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString legacyPath =
        directory.filePath(QStringLiteral("notes.db.txt"));
    QFile legacy(legacyPath);
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    const QByteArray payload =
        QByteArrayLiteral("STICKY_NOTES_C_V1\t1\t8\n"
                          "7\t0\t2026-07-26\t1\t100\t200\tlegacy%20note\n");
    QCOMPARE(legacy.write(payload), payload.size());
    legacy.close();

    Notebook notebook;
    QVERIFY(notebook.open(databasePath(directory)).code == NotebookErrorCode::None);
    const NotebookMutation imported = notebook.importLegacyFile(legacyPath);
    QVERIFY2(imported.ok(), qPrintable(imported.error.message));
    QCOMPARE(imported.affected, 1);
    QCOMPARE(notebook.count(), 1);

    const NotebookOutcome<NotebookNote> note = notebook.note(7);
    QVERIFY(note.ok());
    QCOMPARE(note.value.text, QStringLiteral("legacy note"));
    QVERIFY(note.value.completed);
    QVERIFY(!note.value.seriesId.isEmpty());

    const NotebookMutation invalid =
        notebook.importLegacyFile(directory.filePath(QStringLiteral("missing.txt")));
    QVERIFY(!invalid.ok());
    QCOMPARE(invalid.error.code, NotebookErrorCode::NotFound);
    QCOMPARE(notebook.count(), 1);
}

void NotebookTests::queryTenThousandNotesUnderThreshold() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    Notebook notebook;
    QVERIFY(notebook.open(databasePath(directory)).code == NotebookErrorCode::None);
    NotebookSnapshot snapshot;
    snapshot.notes.reserve(10'000);
    for (int i = 1; i <= 10'000; ++i) {
        NotebookNote note = importedNote(
            i,
            i % 10 == 0
                ? QStringLiteral("needle result %1").arg(i)
                : QStringLiteral("ordinary note %1").arg(i));
        note.completed = i % 3 == 0;
        note.completedAt = note.completed ? 20'000 + i : 0;
        snapshot.notes.append(note);
    }
    const NotebookMutation imported = notebook.importSnapshot(snapshot);
    QVERIFY2(imported.ok(), qPrintable(imported.error.message));
    QCOMPARE(notebook.count(), 10'000);

    NotebookQuery request;
    request.searchText = QStringLiteral("needle");
    request.completion = NotebookCompletion::Any;
    QList<qint64> samples;
    for (int iteration = 0; iteration < 25; ++iteration) {
        QElapsedTimer timer;
        timer.start();
        const NotebookOutcome<QList<NotebookNote>> result = notebook.query(request);
        samples.append(timer.nsecsElapsed() / 1'000'000);
        QVERIFY(result.ok());
        QCOMPARE(result.value.size(), 1'000);
    }
    std::sort(samples.begin(), samples.end());
    const qint64 p95 = samples.at((samples.size() * 95 + 99) / 100 - 1);
    qInfo().noquote()
        << QStringLiteral("10,000 条内存查询 p95=%1 ms").arg(p95);
    QVERIFY2(p95 <= 100,
             qPrintable(QStringLiteral("10,000 条查询 p95=%1 ms，超过 100 ms")
                            .arg(p95)));
}

void NotebookTests::lockedDatabaseRejectsWriteWithoutChangingState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = databasePath(directory);

    Notebook notebook;
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    const NotebookMutation added =
        notebook.add(dailyDraft(QStringLiteral("keep original")), 100);
    QVERIFY(added.ok());

    const QString connectionName =
        QStringLiteral("notebook_lock_%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    {
        QSqlDatabase locker =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        locker.setDatabaseName(path);
        QVERIFY(locker.open());
        QSqlQuery lock(locker);
        QVERIFY2(lock.exec(QStringLiteral("BEGIN IMMEDIATE")),
                 qPrintable(lock.lastError().text()));

        NotebookUpdate update;
        update.text = QStringLiteral("must roll back");
        const NotebookMutation rejected =
            notebook.update(added.noteId, update, 200);
        QVERIFY(!rejected.ok());
        QCOMPARE(rejected.error.code, NotebookErrorCode::Persistence);
        QCOMPARE(notebook.note(added.noteId).value.text,
                 QStringLiteral("keep original"));

        QVERIFY(lock.exec(QStringLiteral("ROLLBACK")));
        locker.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    NotebookUpdate retry;
    retry.text = QStringLiteral("write after unlock");
    QVERIFY(notebook.update(added.noteId, retry, 300).ok());
}

void NotebookTests::readOnlyDatabaseOpenPreservesCurrentNotebook() {
#ifndef Q_OS_WIN
    QSKIP("The production target is Windows 11.");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString currentPath = directory.filePath(QStringLiteral("current.sqlite"));
    const QString readOnlyPath = directory.filePath(QStringLiteral("readonly.sqlite"));

    {
        Notebook seed;
        QVERIFY(seed.open(readOnlyPath).code == NotebookErrorCode::None);
        QVERIFY(seed.add(dailyDraft(QStringLiteral("read only seed")), 100).ok());
    }

    const DWORD originalAttributes =
        GetFileAttributesW(reinterpret_cast<LPCWSTR>(readOnlyPath.utf16()));
    QVERIFY(originalAttributes != INVALID_FILE_ATTRIBUTES);
    QVERIFY(SetFileAttributesW(
        reinterpret_cast<LPCWSTR>(readOnlyPath.utf16()),
        originalAttributes | FILE_ATTRIBUTE_READONLY));
    struct AttributeRestorer {
        QString path;
        DWORD attributes;
        ~AttributeRestorer() {
            SetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()),
                               attributes);
        }
    } restorer{readOnlyPath, originalAttributes};

    Notebook notebook;
    QVERIFY(notebook.open(currentPath).code == NotebookErrorCode::None);
    const NotebookMutation current =
        notebook.add(dailyDraft(QStringLiteral("current remains")), 200);
    QVERIFY(current.ok());

    const NotebookError rejected = notebook.open(readOnlyPath);
    QCOMPARE(rejected.code, NotebookErrorCode::Persistence);
    QCOMPARE(notebook.databasePath(), QFileInfo(currentPath).absoluteFilePath());
    QCOMPARE(notebook.note(current.noteId).value.text,
             QStringLiteral("current remains"));
#endif
}

void NotebookTests::corruptDatabaseOpenPreservesCurrentNotebook() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString currentPath = directory.filePath(QStringLiteral("current.sqlite"));
    const QString corruptPath = directory.filePath(QStringLiteral("corrupt.sqlite"));

    QFile corrupt(corruptPath);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    QCOMPARE(corrupt.write("this is not a sqlite database"), 29);
    corrupt.close();

    Notebook notebook;
    QVERIFY(notebook.open(currentPath).code == NotebookErrorCode::None);
    const NotebookMutation current =
        notebook.add(dailyDraft(QStringLiteral("survives corruption")), 100);
    QVERIFY(current.ok());

    const NotebookError rejected = notebook.open(corruptPath);
    QCOMPARE(rejected.code, NotebookErrorCode::CorruptData);
    QCOMPARE(notebook.databasePath(), QFileInfo(currentPath).absoluteFilePath());
    QCOMPARE(notebook.note(current.noteId).value.text,
             QStringLiteral("survives corruption"));
}

void NotebookTests::diskFullRollsBackFailedWrite() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = databasePath(directory);

    Notebook notebook;
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    QVERIFY(notebook.add(dailyDraft(QStringLiteral("seed")), 100).ok());

    const QString connectionName =
        QStringLiteral("notebook_limit_%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    {
        QSqlDatabase limiter =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        limiter.setDatabaseName(path);
        QVERIFY(limiter.open());
        QSqlQuery query(limiter);
        QVERIFY(query.exec(QStringLiteral("PRAGMA page_count")));
        QVERIFY(query.next());
        const int pageCount = query.value(0).toInt();
        qputenv("CHRONONOTES_TEST_SQLITE_MAX_PAGE_COUNT",
                QByteArray::number(pageCount));
        limiter.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    struct EnvironmentRestorer {
        ~EnvironmentRestorer() {
            qunsetenv("CHRONONOTES_TEST_SQLITE_MAX_PAGE_COUNT");
        }
    } environmentRestorer;
    notebook.close();
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);

    const QString maximum(Notebook::MaxTextLength, QLatin1Char('x'));
    NotebookMutation failed;
    int successfulWrites = 0;
    for (; successfulWrites < 100; ++successfulWrites) {
        const int before = notebook.count();
        failed = notebook.add(
            dailyDraft(maximum, QStringLiteral("2026-08-%1")
                                    .arg(successfulWrites % 28 + 1, 2, 10,
                                         QLatin1Char('0'))),
            200 + successfulWrites);
        if (!failed.ok()) {
            QCOMPARE(notebook.count(), before);
            break;
        }
    }
    QVERIFY2(!failed.ok(), "PRAGMA max_page_count did not force SQLITE_FULL");
    QCOMPARE(failed.error.code, NotebookErrorCode::Persistence);

    const int visibleCount = notebook.count();
    notebook.close();
    QVERIFY(notebook.open(path).code == NotebookErrorCode::None);
    QCOMPARE(notebook.count(), visibleCount);
}

void NotebookTests::failedMigrationKeepsSourceAndCreatesBackup() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = databasePath(directory);
    const QString connectionName =
        QStringLiteral("notebook_migration_%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE notes ("
            "id INTEGER PRIMARY KEY, stage INTEGER NOT NULL, "
            "date_key TEXT NOT NULL, text TEXT NOT NULL, "
            "completed INTEGER NOT NULL, completed_at INTEGER NOT NULL, "
            "created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL, "
            "repeat TEXT NOT NULL DEFAULT '')")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE idx_notes_stage_date_completed (value INTEGER)")));
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=1")));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    Notebook notebook;
    const NotebookError rejected = notebook.open(path);
    QCOMPARE(rejected.code, NotebookErrorCode::Persistence);
    QVERIFY(!notebook.isOpen());
    QVERIFY(!QDir(directory.path())
                 .entryList(QStringList{QStringLiteral(
                                "notes.sqlite.pre-v2-*.bak")},
                            QDir::Files)
                 .isEmpty());

    const QString verifyName =
        QStringLiteral("notebook_verify_%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyName);
        database.setDatabaseName(path);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec(QStringLiteral("PRAGMA table_info(notes)")));
        bool hasSeriesId = false;
        while (query.next()) {
            hasSeriesId = hasSeriesId ||
                          query.value(1).toString() == QStringLiteral("series_id");
        }
        QVERIFY(!hasSeriesId);
        database.close();
    }
    QSqlDatabase::removeDatabase(verifyName);
}

void NotebookTests::persistenceP95StaysUnderThreshold() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Notebook notebook;
    QVERIFY(notebook.open(databasePath(directory)).code == NotebookErrorCode::None);

    QList<qint64> samples;
    samples.reserve(100);
    for (int i = 0; i < 100; ++i) {
        QElapsedTimer timer;
        timer.start();
        const NotebookMutation added =
            notebook.add(dailyDraft(QStringLiteral("persistence sample %1").arg(i)),
                         1'000 + i);
        samples.append(timer.nsecsElapsed() / 1'000'000);
        QVERIFY2(added.ok(), qPrintable(added.error.message));
    }
    std::sort(samples.begin(), samples.end());
    const qint64 p95 = samples.at((samples.size() * 95 + 99) / 100 - 1);
    qInfo().noquote()
        << QStringLiteral("Notebook persistence p95=%1 ms").arg(p95);
    QVERIFY2(p95 <= 50,
             qPrintable(QStringLiteral("Persistence p95=%1 ms exceeds 50 ms")
                            .arg(p95)));
}

QTEST_GUILESS_MAIN(NotebookTests)
#include "notebook_tests.moc"
