#include "qt_note_app.h"
#include "backup_service.h"
#include "note_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTemporaryDir>
#include <QTimer>
#include <QTimeZone>
#include <QUrl>

class QtNoteAppTests : public QObject {
    Q_OBJECT

private slots:
    void cleanup();
    void usesEnvironmentDataDirectory();
    void summarizeAsyncRejectsEmptyRequirement();
    void summarizeContextAsyncUsesProvidedContext();
    void newerSummaryRequestDiscardsOlderCompletion();
    void completingEventMovesRowInsteadOfResettingModel();
    void deletingEventCanBeUndone();
    void togglingEventCanBeUndone();
    void searchFindsEventsAcrossStages();
    void clearingSearchRestoresCurrentStage();
    void archiveRowsAreGroupedBySourceDate();
    void selectingEventExposesDetail();
    void updatingSelectedEventRefreshesDetail();
    void deletingSelectedEventClearsDetail();
    void migratesLegacyTextStoreToSqlite();
    void exportsJsonAndImportsItBack();
    void exportsMarkdownSummary();
    void completingRecurringEventCreatesNextOccurrence();
    void undoingRecurringCompletionRemovesDerivedOccurrence();
    void searchCanFilterByCompletionStateAndExposeHighlight();
    void clearingCompletedAllStagesRemovesCompletedOnly();
    void writesOperationLogForUserActions();
    void operationLogDoesNotStoreNoteBody();
    void operationLogIsCappedAtFiftyMiB();
    void storesSummaryHistoryFromAsyncResult();
    void archiveRowsUseSingleOuterSectionWithDateInMeta();
    void exportsAndImportsJsonUsingChosenFile();
    void exportsMarkdownUsingChosenFile();
    void hasVisibleRowsTracksArchiveAndSearchViews();
    void previewsImportJsonEventCountBeforeImport();
    void uiFontSettingsPersistThroughNoteApp();
    void exposesSystemFontFamiliesForSettings();
    void apiKeyUsesCredentialStoreInsteadOfConfigFile();
    void migratesLegacyPlaintextApiKeyToCredentialStore();
    void preservesUnsupportedFutureConfig();
    void failedConfigSaveRollsBackDraft();
    void failedCredentialSaveRollsBackSettings();
    void refreshesDayAfterMidnightOrResume();
    void refreshesWeekMonthAndYearAcrossBoundaries();
    void undoExpiryUsesInjectedClock();
    void workspaceSnapshotRoundTripsNotesHistoryAndPreferences();
    void invalidWorkspaceSnapshotDoesNotChangeCurrentState();
};

static QTemporaryDir makeIsolatedDataDir() {
    QTemporaryDir dir;
    return dir;
}

static void useDataDir(const QTemporaryDir &dir) {
    QVERIFY(dir.isValid());
    qputenv("STICKY_NOTES_DATA_DIR", QDir::toNativeSeparators(dir.path()).toUtf8());
}

void QtNoteAppTests::cleanup() {
    qunsetenv("STICKY_NOTES_DATA_DIR");
}

void QtNoteAppTests::refreshesDayAfterMidnightOrResume() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    auto clock = std::make_shared<TestClock>(
        QDateTime(QDate(2026, 12, 31), QTime(23, 59, 59), QTimeZone("Asia/Shanghai")));
    NoteApp app(clock);

    QCOMPARE(app.dateKey(), QStringLiteral("2026-12-31"));
    clock->advanceMSecs(2000);
    app.refreshTemporalState();
    QCOMPARE(app.dateKey(), QStringLiteral("2027-01-01"));
}

void QtNoteAppTests::refreshesWeekMonthAndYearAcrossBoundaries() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    auto clock = std::make_shared<TestClock>(
        QDateTime(QDate(2023, 12, 31), QTime(23, 59), QTimeZone("Asia/Shanghai")));
    NoteApp app(clock);

    app.setStage(NOTE_STAGE_WEEK);
    QCOMPARE(app.dateKey(), QStringLiteral("2023-W52"));
    clock->setNow(QDateTime(QDate(2024, 1, 1), QTime(0, 1), QTimeZone("Asia/Shanghai")));
    app.refreshTemporalState();
    QCOMPARE(app.dateKey(), QStringLiteral("2024-W01"));

    app.setStage(NOTE_STAGE_MONTH);
    QCOMPARE(app.dateKey(), QStringLiteral("2024-01"));
    clock->setNow(QDateTime(QDate(2025, 1, 1), QTime(0, 1), QTimeZone("Asia/Shanghai")));
    app.setStage(NOTE_STAGE_YEAR);
    QCOMPARE(app.dateKey(), QStringLiteral("2025"));
}

void QtNoteAppTests::undoExpiryUsesInjectedClock() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    auto clock = std::make_shared<TestClock>(
        QDateTime(QDate(2026, 7, 12), QTime(9, 0), QTimeZone("Asia/Shanghai")));
    NoteApp app(clock);
    app.addEvent(QStringLiteral("限时撤销"));
    const int id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();
    app.deleteEvent(id);
    QVERIFY(app.canUndo());

    clock->advanceMSecs(5001);
    QVERIFY(!app.canUndo());
}

void QtNoteAppTests::usesEnvironmentDataDirectory() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);

    NoteApp app;
    app.addEvent(QStringLiteral("隔离数据"));

    QVERIFY2(QFileInfo::exists(dir.filePath(QStringLiteral("notes.sqlite"))),
             qPrintable(dir.filePath(QStringLiteral("notes.sqlite"))));
}

void QtNoteAppTests::summarizeAsyncRejectsEmptyRequirement() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    QSignalSpy spy(&app, &NoteApp::summaryReady);

    app.summarizeAsync(QString());

    if (spy.count() == 0) {
        QVERIFY(spy.wait(500));
    }
    QCOMPARE(spy.count(), 1);
    const QString result = spy.takeFirst().at(0).toString();
    QVERIFY2(result.contains(QStringLiteral("总结要求")), qPrintable(result));
}

void QtNoteAppTests::summarizeContextAsyncUsesProvidedContext() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    QSignalSpy spy(&app, &NoteApp::summaryReady);

    app.summarizeContextAsync(QStringLiteral("总结项目"), QStringLiteral("项目摘要上下文\n标题：产品重构"));

    if (spy.count() == 0) {
        QVERIFY(spy.wait(5000));
    }
    QCOMPARE(spy.count(), 1);
    QCOMPARE(app.aiState(), QStringLiteral("error"));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("summary-history.md"))));
}


void QtNoteAppTests::completingEventMovesRowInsteadOfResettingModel() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("第一件事"));
    app.addEvent(QStringLiteral("第二件事"));

    QCOMPARE(app.rowCount(), 2);
    const int completed_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();

    QSignalSpy movedSpy(&app, &QAbstractItemModel::rowsMoved);
    QSignalSpy resetSpy(&app, &QAbstractItemModel::modelReset);

    app.toggleEvent(completed_id);

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(movedSpy.count(), 1);
    QCOMPARE(app.rowCount(), 2);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("第一件事"));
    QCOMPARE(app.data(app.index(1, 0), NoteApp::TextRole).toString(), QStringLiteral("第二件事"));
    QCOMPARE(app.data(app.index(1, 0), NoteApp::CompletedRole).toBool(), true);
}

void QtNoteAppTests::deletingEventCanBeUndone() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("误删事件"));
    const int event_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();

    app.deleteEvent(event_id);
    QCOMPARE(app.rowCount(), 0);
    QVERIFY(app.canUndo());

    app.undoLastAction();

    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::IdRole).toInt(), event_id);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("误删事件"));
    QCOMPARE(app.canUndo(), false);
}

void QtNoteAppTests::togglingEventCanBeUndone() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("误完成事件"));
    const int event_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();

    app.toggleEvent(event_id);
    QVERIFY(app.data(app.index(app.rowCount() - 1, 0), NoteApp::CompletedRole).toBool());
    QVERIFY(app.canUndo());

    app.undoLastAction();

    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::IdRole).toInt(), event_id);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::CompletedRole).toBool(), false);
    QCOMPARE(app.canUndo(), false);
}

void QtNoteAppTests::searchFindsEventsAcrossStages() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("alpha-target"));
    app.setStage(NOTE_STAGE_WEEK);
    app.addEvent(QStringLiteral("weekly-plan"));

    app.setSearchQuery(QStringLiteral("target"));

    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("alpha-target"));
    QVERIFY(app.data(app.index(0, 0), NoteApp::MetaRole).toString().contains(QStringLiteral("每天")));
}

void QtNoteAppTests::clearingSearchRestoresCurrentStage() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("daily-only"));
    app.setStage(NOTE_STAGE_WEEK);
    app.addEvent(QStringLiteral("weekly-only"));

    app.setSearchQuery(QStringLiteral("daily"));
    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("daily-only"));

    app.setSearchQuery(QString());

    QCOMPARE(app.rowCount(), 2);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("weekly-only"));
}

void QtNoteAppTests::archiveRowsAreGroupedBySourceDate() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("daily archive candidate"));

    app.setStage(NOTE_STAGE_WEEK);

    bool found_archive = false;
    for (int row = 0; row < app.rowCount(); ++row) {
        if (!app.data(app.index(row, 0), NoteApp::ArchiveRole).toBool()) {
            continue;
        }
        found_archive = true;
        const QString section = app.data(app.index(row, 0), NoteApp::SectionRole).toString();
        QCOMPARE(section, QStringLiteral("自动收纳"));
        QVERIFY2(app.data(app.index(row, 0), NoteApp::MetaRole).toString().contains(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))),
                 qPrintable(app.data(app.index(row, 0), NoteApp::MetaRole).toString()));
    }
    QVERIFY(found_archive);
}

void QtNoteAppTests::selectingEventExposesDetail() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("long detail content"));
    const int event_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();

    app.selectEvent(event_id, false);

    QCOMPARE(app.selectedEventId(), event_id);
    QCOMPARE(app.selectedEventText(), QStringLiteral("long detail content"));
    QCOMPARE(app.hasSelectedEvent(), true);
    QCOMPARE(app.selectedEventReadOnly(), false);
}

void QtNoteAppTests::updatingSelectedEventRefreshesDetail() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("before"));
    const int event_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();
    app.selectEvent(event_id, false);

    app.updateEvent(event_id, QStringLiteral("after"));

    QCOMPARE(app.selectedEventText(), QStringLiteral("after"));
}

void QtNoteAppTests::deletingSelectedEventClearsDetail() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("delete selected"));
    const int event_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();
    app.selectEvent(event_id, false);

    app.deleteEvent(event_id);

    QCOMPARE(app.hasSelectedEvent(), false);
    QCOMPARE(app.selectedEventId(), -1);
}

void QtNoteAppTests::migratesLegacyTextStoreToSqlite() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);

    NoteStore legacy{};
    note_store_init(&legacy);
    const std::wstring today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")).toStdWString();
    note_store_add(&legacy, NOTE_STAGE_DAY, today.c_str(), L"legacy migrated");
    QVERIFY(note_store_save(&legacy, QDir::toNativeSeparators(dir.filePath(QStringLiteral("notes.db.txt"))).toStdWString().c_str()));

    NoteApp app;

    QVERIFY2(QFileInfo::exists(dir.filePath(QStringLiteral("notes.sqlite"))),
             qPrintable(dir.filePath(QStringLiteral("notes.sqlite"))));
    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("legacy migrated"));
}

void QtNoteAppTests::exportsJsonAndImportsItBack() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("json backup item"));

    QVERIFY(app.exportJson());
    QVERIFY(QFileInfo::exists(dir.filePath(QStringLiteral("stickies-export.json"))));

    app.clearAllNotes();
    QCOMPARE(app.rowCount(), 0);

    QVERIFY(app.importJson());
    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("json backup item"));
}

void QtNoteAppTests::exportsMarkdownSummary() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("markdown item"));

    QVERIFY(app.exportMarkdown());

    QFile file(dir.filePath(QStringLiteral("stickies-export.md")));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString markdown = QString::fromUtf8(file.readAll());
    QVERIFY2(markdown.contains(QStringLiteral("markdown item")), qPrintable(markdown));
    QVERIFY2(markdown.contains(QStringLiteral("# ChronoNotes 便签导出")), qPrintable(markdown));
}

void QtNoteAppTests::completingRecurringEventCreatesNextOccurrence() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("daily repeat"));
    const int event_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();

    QVERIFY(app.setEventRepeat(event_id, QStringLiteral("daily")));
    app.toggleEvent(event_id);
    app.setSearchQuery(QStringLiteral("daily repeat"));

    QCOMPARE(app.rowCount(), 2);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::RepeatRole).toString(), QStringLiteral("daily"));
}

void QtNoteAppTests::undoingRecurringCompletionRemovesDerivedOccurrence() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("atomic daily repeat"));
    const int event_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();

    QVERIFY(app.setEventRepeat(event_id, QStringLiteral("daily")));
    app.toggleEvent(event_id);
    app.setSearchQuery(QStringLiteral("atomic daily repeat"));
    QCOMPARE(app.rowCount(), 2);

    app.undoLastAction();

    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::IdRole).toInt(), event_id);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::CompletedRole).toBool(), false);
}

void QtNoteAppTests::searchCanFilterByCompletionStateAndExposeHighlight() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("target open"));
    app.addEvent(QStringLiteral("target done"));
    const int done_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();
    app.toggleEvent(done_id);

    app.setSearchQuery(QStringLiteral("target"));
    QCOMPARE(app.rowCount(), 2);
    QVERIFY(app.data(app.index(0, 0), NoteApp::HighlightedTextRole).toString().contains(QStringLiteral("<mark>target</mark>")));

    app.setSearchCompletionFilter(1);
    QCOMPARE(app.rowCount(), 1);
    QVERIFY(app.data(app.index(0, 0), NoteApp::CompletedRole).toBool());

    app.setSearchCompletionFilter(0);
    QCOMPARE(app.rowCount(), 1);
    QVERIFY(!app.data(app.index(0, 0), NoteApp::CompletedRole).toBool());
}

void QtNoteAppTests::clearingCompletedAllStagesRemovesCompletedOnly() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("done daily"));
    const int done_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();
    app.toggleEvent(done_id);
    app.addEvent(QStringLiteral("open daily"));
    app.setStage(NOTE_STAGE_WEEK);
    app.addEvent(QStringLiteral("open weekly"));

    app.clearCompletedAll();
    app.setSearchQuery(QStringLiteral("daily"));

    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("open daily"));
}

void QtNoteAppTests::writesOperationLogForUserActions() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;

    app.addEvent(QStringLiteral("logged item"));
    const int event_id = app.data(app.index(0, 0), NoteApp::IdRole).toInt();
    app.toggleEvent(event_id);
    app.deleteEvent(event_id);

    QFile file(dir.filePath(QStringLiteral("operations.jsonl")));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString log = QString::fromUtf8(file.readAll());
    QVERIFY2(log.contains(QStringLiteral("\"action\":\"add\"")), qPrintable(log));
    QVERIFY2(log.contains(QStringLiteral("\"action\":\"toggle\"")), qPrintable(log));
    QVERIFY2(log.contains(QStringLiteral("\"action\":\"delete\"")), qPrintable(log));
    QVERIFY2(log.contains(QStringLiteral("\"eventId\":%1").arg(event_id)), qPrintable(log));
}

void QtNoteAppTests::operationLogDoesNotStoreNoteBody() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    const QString private_body = QStringLiteral("private body must stay out of diagnostics");

    app.addEvent(private_body);

    QFile file(dir.filePath(QStringLiteral("operations.jsonl")));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString log = QString::fromUtf8(file.readAll());
    QVERIFY(!log.contains(private_body));
    QVERIFY(!log.contains(QStringLiteral("\"text\"")));
}

void QtNoteAppTests::operationLogIsCappedAtFiftyMiB() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    const QString path = dir.filePath(QStringLiteral("operations.jsonl"));
    QFile oversized(path);
    QVERIFY(oversized.open(QIODevice::ReadWrite));
    QVERIFY(oversized.resize(50LL * 1024LL * 1024LL));
    oversized.close();

    app.addEvent(QStringLiteral("log rotation trigger"));

    QFile rotated(path);
    QVERIFY(rotated.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray contents = rotated.readAll();
    QVERIFY(contents.size() < 16 * 1024);
    QVERIFY(contents.contains("\"action\":\"add\""));
    QVERIFY(!contents.contains("log rotation trigger"));
}

void QtNoteAppTests::storesSummaryHistoryFromAsyncResult() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("history item"));
    QSignalSpy spy(&app, &NoteApp::summaryReady);

    app.summarizeAsync(QStringLiteral("记录历史"));

    if (spy.count() == 0) {
        QVERIFY(spy.wait(1000));
    }
    QCOMPARE(spy.count(), 1);

    QCOMPARE(app.aiState(), QStringLiteral("error"));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("summary-history.md"))));
}

void QtNoteAppTests::archiveRowsUseSingleOuterSectionWithDateInMeta() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("archive child row"));

    app.setStage(NOTE_STAGE_MONTH);

    bool found_archive = false;
    for (int row = 0; row < app.rowCount(); ++row) {
        if (!app.data(app.index(row, 0), NoteApp::ArchiveRole).toBool()) {
            continue;
        }
        found_archive = true;
        QCOMPARE(app.data(app.index(row, 0), NoteApp::SectionRole).toString(), QStringLiteral("自动收纳"));
        QVERIFY2(app.data(app.index(row, 0), NoteApp::MetaRole).toString().contains(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))),
                 qPrintable(app.data(app.index(row, 0), NoteApp::MetaRole).toString()));
    }
    QVERIFY(found_archive);
}

void QtNoteAppTests::exportsAndImportsJsonUsingChosenFile() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    const QString chosen = dir.filePath(QStringLiteral("chosen-backup.json"));

    NoteApp app;
    app.addEvent(QStringLiteral("chosen json item"));

    QVERIFY(app.exportJsonToFile(QUrl::fromLocalFile(chosen)));
    QVERIFY2(QFileInfo::exists(chosen), qPrintable(chosen));

    app.clearAllNotes();
    QCOMPARE(app.rowCount(), 0);

    QVERIFY(app.importJsonFromFile(QUrl::fromLocalFile(chosen)));
    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("chosen json item"));
}

void QtNoteAppTests::exportsMarkdownUsingChosenFile() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    const QString chosen = dir.filePath(QStringLiteral("chosen-notes.md"));

    NoteApp app;
    app.addEvent(QStringLiteral("chosen markdown item"));

    QVERIFY(app.exportMarkdownToFile(QUrl::fromLocalFile(chosen)));

    QFile file(chosen);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString markdown = QString::fromUtf8(file.readAll());
    QVERIFY2(markdown.contains(QStringLiteral("chosen markdown item")), qPrintable(markdown));
}

void QtNoteAppTests::hasVisibleRowsTracksArchiveAndSearchViews() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);

    NoteApp app;
    QCOMPARE(app.hasVisibleRows(), false);

    app.addEvent(QStringLiteral("visible row"));
    QCOMPARE(app.hasVisibleRows(), true);

    app.setStage(NOTE_STAGE_MONTH);
    QCOMPARE(app.hasVisibleRows(), true);

    app.setSearchQuery(QStringLiteral("nothing-matches-this"));
    QCOMPARE(app.hasVisibleRows(), false);
}

void QtNoteAppTests::previewsImportJsonEventCountBeforeImport() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    const QString chosen = dir.filePath(QStringLiteral("preview-backup.json"));

    NoteApp app;
    app.addEvent(QStringLiteral("preview json item"));

    QVERIFY(app.exportJsonToFile(QUrl::fromLocalFile(chosen)));
    QCOMPARE(app.previewImportJsonEventCount(QUrl::fromLocalFile(chosen)), 1);
    QCOMPARE(app.previewImportJsonEventCount(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.json")))), -1);

    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(), QStringLiteral("preview json item"));
}

void QtNoteAppTests::uiFontSettingsPersistThroughNoteApp() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);

    {
        NoteApp app;
        QCOMPARE(app.uiFontFamily(), QStringLiteral("Microsoft YaHei UI"));
        QCOMPARE(app.uiFontSize(), 14);
        app.setUiFontFamily(QStringLiteral("Segoe UI"));
        app.setUiFontSize(15);
        app.saveConfig();
    }

    NoteApp reloaded;
    QCOMPARE(reloaded.uiFontFamily(), QStringLiteral("Segoe UI"));
    QCOMPARE(reloaded.uiFontSize(), 15);

    reloaded.setUiFontSize(99);
    QCOMPARE(reloaded.uiFontSize(), 14);
}

void QtNoteAppTests::exposesSystemFontFamiliesForSettings() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);

    NoteApp app;
    const QStringList families = app.uiFontFamilies();

    QVERIFY(!families.isEmpty());
    QVERIFY2(families.contains(app.uiFontFamily()), qPrintable(app.uiFontFamily()));
}

void QtNoteAppTests::apiKeyUsesCredentialStoreInsteadOfConfigFile() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);

    {
        NoteApp app;
        QVERIFY(!app.hasApiKey());
        QVERIFY(app.replaceApiKey(QStringLiteral("test-secret-never-write-plain")));
        QVERIFY(app.hasApiKey());
        QVERIFY(app.saveConfig());
    }

    QFile config(dir.filePath(QStringLiteral("config.ini")));
    QVERIFY(config.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(config.readAll());
    QVERIFY(!contents.contains(QStringLiteral("test-secret-never-write-plain")));
    QVERIFY(!contents.contains(QStringLiteral("api_key=")));

    NoteApp reloaded;
    QVERIFY(reloaded.hasApiKey());
    QVERIFY(reloaded.clearApiKey());
    QVERIFY(!reloaded.hasApiKey());
}

void QtNoteAppTests::migratesLegacyPlaintextApiKeyToCredentialStore() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    QFile legacyConfig(dir.filePath(QStringLiteral("config.ini")));
    QVERIFY(legacyConfig.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray legacy =
        "version=1\n"
        "api_url=https://example.com/v1/chat/completions\n"
        "api_key=legacy-plaintext-secret\n"
        "model=legacy-model\n";
    QCOMPARE(legacyConfig.write(legacy), legacy.size());
    legacyConfig.close();

    NoteApp app;
    QVERIFY(app.hasApiKey());

    QFile migratedConfig(dir.filePath(QStringLiteral("config.ini")));
    QVERIFY(migratedConfig.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray migrated = migratedConfig.readAll();
    QVERIFY(!migrated.contains("legacy-plaintext-secret"));
    QVERIFY(!migrated.contains("api_key="));
    QVERIFY(app.clearApiKey());
}

void QtNoteAppTests::preservesUnsupportedFutureConfig() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    const QByteArray original =
        QByteArrayLiteral("version=999\nmodel=future-model\n");
    QFile config(dir.filePath(QStringLiteral("config.ini")));
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(config.write(original), original.size());
    config.close();

    NoteApp app;
    QVERIFY(app.notice().contains(QStringLiteral("更高版本")));
    QCOMPARE(app.modelName(), QString());
    app.setModelName(QStringLiteral("attempted-overwrite"));
    QVERIFY(!app.saveConfig());
    QCOMPARE(app.modelName(), QString());

    QVERIFY(config.open(QIODevice::ReadOnly));
    QCOMPARE(config.readAll(), original);
}

void QtNoteAppTests::failedConfigSaveRollsBackDraft() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    const QString persistedUrl = app.apiUrl();

    app.setApiUrl(QString(AppConfig::MaximumUrlLength + 1,
                          QLatin1Char('x')));
    QVERIFY(!app.saveConfig());
    QCOMPARE(app.apiUrl(), persistedUrl);

    QFile config(dir.filePath(QStringLiteral("config.ini")));
    QVERIFY(config.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(config.readAll());
    QVERIFY(contents.contains(persistedUrl));
    QVERIFY(!contents.contains(QString(AppConfig::MaximumUrlLength + 1,
                                       QLatin1Char('x'))));
}

void QtNoteAppTests::failedCredentialSaveRollsBackSettings() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    const QString originalUrl = app.apiUrl();
    app.setApiUrl(QStringLiteral(
        "https://changed.example/v1/chat/completions"));

    QVERIFY(!app.saveSettings(QString(3000, QLatin1Char('k'))));
    QCOMPARE(app.apiUrl(), originalUrl);
    QVERIFY(!app.hasApiKey());

    QFile config(dir.filePath(QStringLiteral("config.ini")));
    QVERIFY(config.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(config.readAll());
    QVERIFY(contents.contains(originalUrl));
    QVERIFY(!contents.contains(QStringLiteral("changed.example")));
}

void QtNoteAppTests::workspaceSnapshotRoundTripsNotesHistoryAndPreferences() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("workspace note"));
    app.setApiUrl(QStringLiteral("https://example.com/v1/chat/completions"));
    app.setModelName(QStringLiteral("production-model"));
    app.setAllowLocalHttp(true);
    app.setReduceMotion(true);
    app.setUiFontFamily(QStringLiteral("Segoe UI"));
    app.setUiFontSize(16);
    QVERIFY(app.saveConfig());
    QVERIFY(app.replaceApiKey(QStringLiteral("secret-never-enters-workspace")));

    QFile history(dir.filePath(QStringLiteral("summary-history.md")));
    QVERIFY(history.open(QIODevice::WriteOnly | QIODevice::Text));
    QCOMPARE(history.write("## preserved summary\n"), 21);
    history.close();

    BackupService::WorkspaceSnapshot snapshot;
    QString error;
    QVERIFY2(app.readWorkspaceSnapshot(&snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.notes.size(), 1);
    QCOMPARE(snapshot.summaries.size(), 1);
    QCOMPARE(snapshot.preferences.value(QStringLiteral("model")).toString(),
             QStringLiteral("production-model"));
    QVERIFY(!snapshot.preferences.contains(QStringLiteral("apiKey")));
    const QByteArray encoded = BackupService::encodeWorkspaceV2(snapshot, &error);
    QVERIFY2(!encoded.isEmpty(), qPrintable(error));
    QVERIFY(!encoded.contains("secret-never-enters-workspace"));

    app.clearAllNotes();
    app.setModelName(QStringLiteral("changed"));
    app.setAllowLocalHttp(false);
    QVERIFY2(app.applyWorkspaceSnapshot(snapshot, &error), qPrintable(error));
    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(),
             QStringLiteral("workspace note"));
    QCOMPARE(app.modelName(), QStringLiteral("production-model"));
    QCOMPARE(app.allowLocalHttp(), true);
    QCOMPARE(app.reduceMotion(), true);
    QCOMPARE(app.uiFontFamily(), QStringLiteral("Segoe UI"));
    QCOMPARE(app.uiFontSize(), 16);
    QVERIFY(app.hasApiKey());

    QFile restoredHistory(dir.filePath(QStringLiteral("summary-history.md")));
    QVERIFY(restoredHistory.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(restoredHistory.readAll(), QByteArray("## preserved summary\n"));
    QVERIFY(app.clearApiKey());
}

void QtNoteAppTests::invalidWorkspaceSnapshotDoesNotChangeCurrentState() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);
    NoteApp app;
    app.addEvent(QStringLiteral("must survive invalid import"));

    BackupService::WorkspaceSnapshot snapshot;
    QString error;
    QVERIFY2(app.readWorkspaceSnapshot(&snapshot, &error), qPrintable(error));
    snapshot.notes[0].text = QString(Notebook::MaxTextLength + 1,
                                     QLatin1Char('x'));

    QVERIFY(!app.applyWorkspaceSnapshot(snapshot, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(app.rowCount(), 1);
    QCOMPARE(app.data(app.index(0, 0), NoteApp::TextRole).toString(),
             QStringLiteral("must survive invalid import"));
}

void QtNoteAppTests::newerSummaryRequestDiscardsOlderCompletion() {
    QTemporaryDir dir = makeIsolatedDataDir();
    useDataDir(dir);

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    int receivedRequests = 0;
    connect(&server, &QTcpServer::newConnection, &server,
            [&server, &receivedRequests]() {
        while (server.hasPendingConnections()) {
            QTcpSocket *socket = server.nextPendingConnection();
            socket->setParent(&server);
            connect(socket, &QTcpSocket::readyRead, socket,
                    [socket, &receivedRequests]() {
                socket->readAll();
                if (socket->property("responseScheduled").toBool()) {
                    return;
                }
                socket->setProperty("responseScheduled", true);
                const int requestNumber = ++receivedRequests;
                const QByteArray content =
                    requestNumber == 1 ? QByteArrayLiteral("older result")
                                       : QByteArrayLiteral("newer result");
                const QByteArray payload =
                    QByteArrayLiteral(R"({"choices":[{"message":{"content":")") +
                    content + QByteArrayLiteral(R"("}}]})");
                const int delayMs = requestNumber == 1 ? 600 : 20;
                QTimer::singleShot(delayMs, socket, [socket, payload]() {
                    if (socket->state() == QAbstractSocket::UnconnectedState) {
                        return;
                    }
                    const QByteArray response =
                        QByteArrayLiteral("HTTP/1.1 200 OK\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Connection: close\r\n"
                                          "Content-Length: ") +
                        QByteArray::number(payload.size()) +
                        QByteArrayLiteral("\r\n\r\n") + payload;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            });
        }
    });

    NoteApp app;
    app.setApiUrl(QStringLiteral("http://127.0.0.1:%1/v1/chat/completions")
                      .arg(server.serverPort()));
    app.setModelName(QStringLiteral("test-model"));
    app.setAllowLocalHttp(true);
    QVERIFY(app.replaceApiKey(QStringLiteral("test-secret")));
    QSignalSpy summarySpy(&app, &NoteApp::summaryReady);

    app.summarizeContextAsync(QStringLiteral("first request"),
                              QStringLiteral("immutable first context"));
    QTRY_COMPARE_WITH_TIMEOUT(receivedRequests, 1, 3000);
    app.summarizeContextAsync(QStringLiteral("second request"),
                              QStringLiteral("immutable second context"));
    QTRY_COMPARE_WITH_TIMEOUT(receivedRequests, 2, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(summarySpy.count(), 1, 3000);
    QCOMPARE(summarySpy.first().first().toString(), QStringLiteral("newer result"));
    QCOMPARE(app.aiState(), QStringLiteral("success"));

    QTest::qWait(700);
    QCOMPARE(summarySpy.count(), 1);
    QCOMPARE(app.aiState(), QStringLiteral("success"));
}

QTEST_MAIN(QtNoteAppTests)

#include "qt_note_app_tests.moc"
