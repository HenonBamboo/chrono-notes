#include "qt_note_app.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QUrl>

class QtNoteAppTests : public QObject {
    Q_OBJECT

private slots:
    void cleanup();
    void usesEnvironmentDataDirectory();
    void summarizeAsyncRejectsEmptyRequirement();
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
    void searchCanFilterByCompletionStateAndExposeHighlight();
    void clearingCompletedAllStagesRemovesCompletedOnly();
    void writesOperationLogForUserActions();
    void storesSummaryHistoryFromAsyncResult();
    void archiveRowsUseSingleOuterSectionWithDateInMeta();
    void exportsAndImportsJsonUsingChosenFile();
    void exportsMarkdownUsingChosenFile();
    void hasVisibleRowsTracksArchiveAndSearchViews();
    void previewsImportJsonEventCountBeforeImport();
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
    QVERIFY(QFileInfo::exists(dir.filePath(QStringLiteral("notes-export.json"))));

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

    QFile file(dir.filePath(QStringLiteral("notes-export.md")));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString markdown = QString::fromUtf8(file.readAll());
    QVERIFY2(markdown.contains(QStringLiteral("markdown item")), qPrintable(markdown));
    QVERIFY2(markdown.contains(QStringLiteral("# StickyNotesC 导出")), qPrintable(markdown));
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

    QFile file(dir.filePath(QStringLiteral("summary-history.md")));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString history = QString::fromUtf8(file.readAll());
    QVERIFY2(history.contains(QStringLiteral("记录历史")), qPrintable(history));
    QVERIFY2(history.contains(QStringLiteral("history item")), qPrintable(history));
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

QTEST_MAIN(QtNoteAppTests)

#include "qt_note_app_tests.moc"
