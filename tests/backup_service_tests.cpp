#include "backup_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class BackupServiceTests : public QObject {
    Q_OBJECT

private slots:
    void validV1PreviewMatchesCommittedImport();
    void invalidV1BackupIsRejectedWithoutReplacingStore_data();
    void invalidV1BackupIsRejectedWithoutReplacingStore();
    void v1ImportRejectsFilesLargerThanFiftyMiB();
    void exportsV1JsonAndMarkdownAtomically();
    void notebookSnapshotRoundTripsV1WithoutNoteStore();
    void notebookSnapshotRejectsInvalidDataWithoutChangingTarget();
    void notebookSnapshotExportsMarkdown();
    void workspaceV2RoundTripsAndExcludesCredentials();
    void workspaceV2RejectsInvalidNotes();
};

namespace {

QJsonObject validEvent(int id = 1) {
    QJsonObject event;
    event.insert(QStringLiteral("id"), id);
    event.insert(QStringLiteral("stage"), NOTE_STAGE_DAY);
    event.insert(QStringLiteral("dateKey"), QStringLiteral("2026-07-12"));
    event.insert(QStringLiteral("text"), QStringLiteral("Production-safe backup"));
    event.insert(QStringLiteral("completed"), false);
    event.insert(QStringLiteral("completedAt"), QStringLiteral("0"));
    event.insert(QStringLiteral("createdAt"), QStringLiteral("100"));
    event.insert(QStringLiteral("updatedAt"), QStringLiteral("100"));
    event.insert(QStringLiteral("repeat"), QStringLiteral(""));
    event.insert(QStringLiteral("seriesId"), QStringLiteral("series-%1").arg(id));
    return event;
}

QJsonObject v1Document(const QJsonArray &events, int version = 1) {
    QJsonObject root;
    root.insert(QStringLiteral("version"), version);
    root.insert(QStringLiteral("events"), events);
    return root;
}

bool writeJson(const QString &path, const QJsonObject &object) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray bytes = QJsonDocument(object).toJson();
    return file.write(bytes) == bytes.size();
}

NotebookNote notebookNote(int id = 1) {
    NotebookNote note;
    note.id = id;
    note.stage = NotebookStage::Day;
    note.dateKey = QStringLiteral("2026-07-12");
    note.text = QStringLiteral("Notebook-native backup");
    note.createdAt = 100;
    note.updatedAt = 110;
    note.repeat = NotebookRepeat::Daily;
    note.seriesId = QStringLiteral("series-%1").arg(id);
    return note;
}

}

void BackupServiceTests::validV1PreviewMatchesCommittedImport() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("notes.json"));
    QVERIFY(writeJson(filePath, v1Document(QJsonArray{validEvent(7), validEvent(9)})));

    QString error;
    QCOMPARE(BackupService::previewJsonEventCount(QUrl::fromLocalFile(filePath), &error), 2);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    NoteStore store{};
    note_store_init(&store);
    QString importedPath;
    QVERIFY2(BackupService::importJson(QUrl::fromLocalFile(filePath), &store,
                                       &importedPath, &error), qPrintable(error));
    QCOMPARE(store.count, 2);
    QCOMPARE(importedPath, filePath);
    QCOMPARE(store.items.at(0).id, 7);
    QCOMPARE(QString::fromWCharArray(store.items.at(1).text),
             QStringLiteral("Production-safe backup"));
}

void BackupServiceTests::invalidV1BackupIsRejectedWithoutReplacingStore_data() {
    QTest::addColumn<QJsonObject>("document");

    QTest::newRow("unsupported-version") << v1Document(QJsonArray{validEvent()}, 2);

    QJsonObject invalidStage = validEvent();
    invalidStage.insert(QStringLiteral("stage"), 99);
    QTest::newRow("invalid-stage") << v1Document(QJsonArray{invalidStage});

    QJsonObject missingCompleted = validEvent();
    missingCompleted.remove(QStringLiteral("completed"));
    QTest::newRow("missing-field") << v1Document(QJsonArray{missingCompleted});

    QJsonObject overlongText = validEvent();
    overlongText.insert(QStringLiteral("text"), QString(65537, QLatin1Char('x')));
    QTest::newRow("overlong-text") << v1Document(QJsonArray{overlongText});

    QTest::newRow("duplicate-id") << v1Document(QJsonArray{validEvent(4), validEvent(4)});
}

void BackupServiceTests::invalidV1BackupIsRejectedWithoutReplacingStore() {
    QFETCH(QJsonObject, document);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("invalid.json"));
    QVERIFY(writeJson(filePath, document));

    QString error;
    QCOMPARE(BackupService::previewJsonEventCount(QUrl::fromLocalFile(filePath), &error), -1);
    QVERIFY(!error.isEmpty());

    NoteStore store{};
    note_store_init(&store);
    QVERIFY(note_store_add(&store, NOTE_STAGE_DAY, L"2026-07-12", L"Existing note") != nullptr);
    const int existingId = store.items.front().id;
    QVERIFY(!BackupService::importJson(QUrl::fromLocalFile(filePath), &store, nullptr, &error));
    QCOMPARE(store.count, 1);
    QCOMPARE(store.items.front().id, existingId);
    QCOMPARE(QString::fromWCharArray(store.items.front().text), QStringLiteral("Existing note"));
}

void BackupServiceTests::v1ImportRejectsFilesLargerThanFiftyMiB() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("oversized.json"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(50LL * 1024LL * 1024LL + 1));
    file.close();

    QString error;
    QCOMPARE(BackupService::previewJsonEventCount(QUrl::fromLocalFile(filePath), &error), -1);
    QVERIFY(error.contains(QStringLiteral("50 MiB")));
}

void BackupServiceTests::exportsV1JsonAndMarkdownAtomically() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    NoteStore store{};
    note_store_init(&store);
    NoteEvent *event = note_store_add(&store, NOTE_STAGE_DAY, L"2026-07-12", L"Export me");
    QVERIFY(event != nullptr);
    QVERIFY(note_store_set_repeat(&store, event->id, L"daily"));

    QString path;
    QString error;
    const QString jsonPath = dir.filePath(QStringLiteral("notes.json"));
    QVERIFY2(BackupService::exportJson(store, QUrl::fromLocalFile(jsonPath), &path, &error),
             qPrintable(error));
    QCOMPARE(path, jsonPath);
    QFile jsonFile(jsonPath);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    const QJsonObject item = QJsonDocument::fromJson(jsonFile.readAll()).object()
                                 .value(QStringLiteral("events")).toArray().at(0).toObject();
    QCOMPARE(item.value(QStringLiteral("seriesId")).toString(),
             QString::fromWCharArray(store.items.front().series_id));

    const QString markdownPath = dir.filePath(QStringLiteral("notes.md"));
    QVERIFY2(BackupService::exportMarkdown(store, QUrl::fromLocalFile(markdownPath), &path, &error),
             qPrintable(error));
    QFile markdownFile(markdownPath);
    QVERIFY(markdownFile.open(QIODevice::ReadOnly));
    const QString markdown = QString::fromUtf8(markdownFile.readAll());
    QVERIFY(markdown.contains(QStringLiteral("Export me")));
    QVERIFY(markdown.contains(QStringLiteral("daily")));
}

void BackupServiceTests::notebookSnapshotRoundTripsV1WithoutNoteStore() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    NotebookSnapshot source;
    source.notes = {notebookNote(7), notebookNote(9)};
    source.notes[1].stage = NotebookStage::Week;
    source.notes[1].dateKey = QStringLiteral("2026-W28");
    source.notes[1].repeat = NotebookRepeat::None;
    source.notes[1].completed = true;
    source.notes[1].completedAt = 120;
    source.notes[1].updatedAt = 120;

    QString error;
    QString exportedPath;
    const QString filePath = dir.filePath(QStringLiteral("notebook.json"));
    QVERIFY2(BackupService::exportJson(source, QUrl::fromLocalFile(filePath),
                                       &exportedPath, &error), qPrintable(error));
    QCOMPARE(exportedPath, filePath);
    QCOMPARE(BackupService::previewJsonEventCount(QUrl::fromLocalFile(filePath), &error), 2);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    NotebookSnapshot imported;
    QString importedPath;
    QVERIFY2(BackupService::importJson(QUrl::fromLocalFile(filePath), &imported,
                                       &importedPath, &error), qPrintable(error));
    QCOMPARE(importedPath, filePath);
    QCOMPARE(imported.schemaVersion, 1);
    QCOMPARE(imported.notes.size(), 2);
    QCOMPARE(imported.notes.at(0), source.notes.at(0));
    QCOMPARE(imported.notes.at(1), source.notes.at(1));
}

void BackupServiceTests::notebookSnapshotRejectsInvalidDataWithoutChangingTarget() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    NotebookSnapshot invalidExport;
    invalidExport.notes = {notebookNote()};
    invalidExport.notes.front().stage = NotebookStage::Week;
    invalidExport.notes.front().dateKey = QStringLiteral("2026-W28");
    invalidExport.notes.front().repeat = NotebookRepeat::Daily;

    QString error;
    const QString exportPath = dir.filePath(QStringLiteral("invalid-export.json"));
    QVERIFY(!BackupService::exportJson(invalidExport, QUrl::fromLocalFile(exportPath),
                                       nullptr, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(exportPath));

    QJsonObject invalidEvent = validEvent(5);
    invalidEvent.insert(QStringLiteral("dateKey"), QStringLiteral("2026-02-30"));
    const QString importPath = dir.filePath(QStringLiteral("invalid-import.json"));
    QVERIFY(writeJson(importPath, v1Document(QJsonArray{invalidEvent})));
    QCOMPARE(BackupService::previewJsonEventCount(QUrl::fromLocalFile(importPath), &error), -1);
    QVERIFY(!error.isEmpty());

    NotebookSnapshot target;
    target.notes = {notebookNote(99)};
    const NotebookSnapshot before = target;
    QVERIFY(!BackupService::importJson(QUrl::fromLocalFile(importPath), nullptr,
                                       nullptr, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!BackupService::importJson(QUrl::fromLocalFile(importPath), &target,
                                       nullptr, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(target.schemaVersion, before.schemaVersion);
    QCOMPARE(target.notes, before.notes);
}

void BackupServiceTests::notebookSnapshotExportsMarkdown() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    NotebookSnapshot snapshot;
    snapshot.notes = {notebookNote()};
    snapshot.notes.front().completed = true;
    snapshot.notes.front().completedAt = 120;
    snapshot.notes.front().updatedAt = 120;

    QString error;
    QString exportedPath;
    const QString filePath = dir.filePath(QStringLiteral("notebook.md"));
    QVERIFY2(BackupService::exportMarkdown(snapshot, QUrl::fromLocalFile(filePath),
                                           &exportedPath, &error), qPrintable(error));
    QCOMPARE(exportedPath, filePath);
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString markdown = QString::fromUtf8(file.readAll());
    QVERIFY(markdown.contains(QStringLiteral("## 每天")));
    QVERIFY(markdown.contains(QStringLiteral("- [x] 2026-07-12 Notebook-native backup")));
    QVERIFY(markdown.contains(QStringLiteral("(repeat: daily)")));
}

void BackupServiceTests::workspaceV2RoundTripsAndExcludesCredentials() {
    BackupService::WorkspaceSnapshot source;
    source.createdAt = QStringLiteral("2026-07-12T10:00:00Z");
    source.appVersion = QStringLiteral("1.0.0");
    BackupService::WorkspaceNote note;
    note.id = 3;
    note.stage = NOTE_STAGE_WEEK;
    note.dateKey = QStringLiteral("2026-W28");
    note.text = QStringLiteral("Workspace note");
    note.createdAt = 10;
    note.updatedAt = 12;
    note.seriesId = QStringLiteral("series-3");
    source.notes.append(note);
    source.projects = QJsonObject{{QStringLiteral("version"), 1},
                                  {QStringLiteral("nextId"), 1},
                                  {QStringLiteral("nodes"), QJsonArray{}}};
    source.summaries = QJsonArray{QJsonObject{{QStringLiteral("result"),
                                               QStringLiteral("Summary")}}};
    source.preferences = QJsonObject{{QStringLiteral("fontSize"), 14},
                                     {QStringLiteral("apiKey"), QStringLiteral("secret")},
                                     {QStringLiteral("credential"), QStringLiteral("secret-2")}};

    QString error;
    const QByteArray encoded = BackupService::encodeWorkspaceV2(source, &error);
    QVERIFY2(!encoded.isEmpty(), qPrintable(error));
    QVERIFY(!encoded.contains("secret"));

    BackupService::WorkspaceSnapshot decoded;
    QVERIFY2(BackupService::decodeWorkspaceV2(encoded, &decoded, &error), qPrintable(error));
    QCOMPARE(decoded.notes.size(), 1);
    QCOMPARE(decoded.notes.front().text, QStringLiteral("Workspace note"));
    QCOMPARE(decoded.projects, source.projects);
    QCOMPARE(decoded.summaries, source.summaries);
    QCOMPARE(decoded.preferences.value(QStringLiteral("fontSize")).toInt(), 14);
    QVERIFY(!decoded.preferences.contains(QStringLiteral("apiKey")));
    QVERIFY(!decoded.preferences.contains(QStringLiteral("credential")));
}

void BackupServiceTests::workspaceV2RejectsInvalidNotes() {
    BackupService::WorkspaceSnapshot snapshot;
    snapshot.createdAt = QStringLiteral("2026-07-12T10:00:00Z");
    snapshot.appVersion = QStringLiteral("1.0.0");
    snapshot.projects = QJsonObject{{QStringLiteral("version"), 1},
                                    {QStringLiteral("nodes"), QJsonArray{}}};
    BackupService::WorkspaceNote first;
    first.id = 1;
    first.stage = NOTE_STAGE_DAY;
    first.dateKey = QStringLiteral("2026-07-12");
    first.text = QStringLiteral("One");
    first.createdAt = 1;
    first.updatedAt = 1;
    first.seriesId = QStringLiteral("series-1");
    snapshot.notes.append(first);
    snapshot.notes.append(first);

    QString error;
    QVERIFY(BackupService::encodeWorkspaceV2(snapshot, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(BackupServiceTests)

#include "backup_service_tests.moc"
