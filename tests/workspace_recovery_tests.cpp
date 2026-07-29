#include "workspace_recovery.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class WorkspaceRecoveryTests : public QObject {
    Q_OBJECT

private slots:
    void createsVerifiedBackupAndPreviewsImpact();
    void invalidImportDoesNotReadOrWriteWorkspace();
    void importCreatesSafetyBackupAndCommitsOnce();
    void failedImportRollsBackOriginalSnapshot();
    void previewRejectsChangedSourceOrCurrentWorkspace();
    void keepsSevenDailyAndThreeDangerousBackups();
    void enforcesTwoHundredMiBManagedBackupLimit();
    void diagnosticsAreStructuredAndStrictlyRedacted();
};

namespace {

BackupService::WorkspaceSnapshot snapshotWithNote(int id, const QString &text) {
    BackupService::WorkspaceSnapshot snapshot;
    snapshot.createdAt = QStringLiteral("2026-07-12T10:00:00Z");
    snapshot.appVersion = QStringLiteral("1.0.0");
    BackupService::WorkspaceNote note;
    note.id = id;
    note.stage = NOTE_STAGE_DAY;
    note.dateKey = QStringLiteral("2026-07-12");
    note.text = text;
    note.createdAt = 100;
    note.updatedAt = 100;
    note.seriesId = QStringLiteral("series-%1").arg(id);
    snapshot.notes.append(note);
    snapshot.projects = QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("nextId"), 1},
        {QStringLiteral("nodes"), QJsonArray{}},
    };
    snapshot.summaries = QJsonArray{};
    snapshot.preferences = QJsonObject{
        {QStringLiteral("fontSize"), 14},
        {QStringLiteral("apiKey"), QStringLiteral("must-not-leak")},
    };
    return snapshot;
}

bool exportSnapshot(const QString &path,
                    const BackupService::WorkspaceSnapshot &snapshot) {
    QString error;
    return BackupService::exportWorkspaceV2(
        snapshot, QUrl::fromLocalFile(path), nullptr, &error);
}

QFileInfoList backupFiles(const QString &path, const QString &pattern) {
    return QDir(path).entryInfoList(QStringList{pattern}, QDir::Files, QDir::Name);
}

}  // namespace

void WorkspaceRecoveryTests::createsVerifiedBackupAndPreviewsImpact() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupService::WorkspaceSnapshot current =
        snapshotWithNote(1, QStringLiteral("Current"));
    WorkspaceRecovery recovery(directory.filePath(QStringLiteral("backups")),
                               QStringLiteral("1.0.0"));
    recovery.setClockForTesting([] {
        return QDateTime::fromString(QStringLiteral("2026-07-26T09:30:00Z"),
                                     Qt::ISODate);
    });
    recovery.setSnapshotAccessors(
        [&](BackupService::WorkspaceSnapshot *snapshot, QString *) {
            *snapshot = current;
            return true;
        },
        [&](const BackupService::WorkspaceSnapshot &snapshot, QString *) {
            current = snapshot;
            return true;
        });

    const QString path = recovery.createBackup();
    QVERIFY2(!path.isEmpty(), qPrintable(recovery.lastError()));
    QVERIFY(QFileInfo::exists(path));
    QCOMPARE(backupFiles(recovery.backupDirectory(), QStringLiteral("manual-*.chrononotes"))
                 .size(),
             1);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    QVERIFY(!bytes.contains("must-not-leak"));

    BackupService::WorkspaceSnapshot imported = snapshotWithNote(2, QStringLiteral("Imported"));
    imported.notes.append(snapshotWithNote(3, QStringLiteral("Second")).notes.front());
    const QString importPath = directory.filePath(QStringLiteral("incoming.chrononotes"));
    QVERIFY(exportSnapshot(importPath, imported));
    const QVariantMap preview =
        recovery.previewImport(QUrl::fromLocalFile(importPath));
    QVERIFY(preview.value(QStringLiteral("valid")).toBool());
    QCOMPARE(preview.value(QStringLiteral("noteCount")).toInt(), 2);
    QCOMPARE(preview.value(QStringLiteral("currentNoteCount")).toInt(), 1);
    QCOMPARE(preview.value(QStringLiteral("noteDelta")).toInt(), 1);
    QVERIFY(preview.value(QStringLiteral("willReplace")).toBool());
}

void WorkspaceRecoveryTests::invalidImportDoesNotReadOrWriteWorkspace() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("invalid.chrononotes"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("not-json"), 8);
    file.close();

    int reads = 0;
    int writes = 0;
    WorkspaceRecovery recovery(directory.filePath(QStringLiteral("backups")),
                               QStringLiteral("1.0.0"));
    recovery.setSnapshotAccessors(
        [&](BackupService::WorkspaceSnapshot *, QString *) {
            ++reads;
            return true;
        },
        [&](const BackupService::WorkspaceSnapshot &, QString *) {
            ++writes;
            return true;
        });

    const QVariantMap preview = recovery.previewImport(QUrl::fromLocalFile(path));
    QVERIFY(!preview.value(QStringLiteral("valid")).toBool());
    QCOMPARE(reads, 0);
    QCOMPARE(writes, 0);
    QVERIFY(!recovery.importWorkspace(QUrl::fromLocalFile(path)));
    QCOMPARE(reads, 0);
    QCOMPARE(writes, 0);
}

void WorkspaceRecoveryTests::importCreatesSafetyBackupAndCommitsOnce() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupService::WorkspaceSnapshot current =
        snapshotWithNote(1, QStringLiteral("Original"));
    const BackupService::WorkspaceSnapshot imported =
        snapshotWithNote(2, QStringLiteral("Imported"));
    const QString importPath = directory.filePath(QStringLiteral("incoming.chrononotes"));
    QVERIFY(exportSnapshot(importPath, imported));

    int writes = 0;
    WorkspaceRecovery recovery(directory.filePath(QStringLiteral("backups")),
                               QStringLiteral("1.0.0"));
    recovery.setSnapshotAccessors(
        [&](BackupService::WorkspaceSnapshot *snapshot, QString *) {
            *snapshot = current;
            return true;
        },
        [&](const BackupService::WorkspaceSnapshot &snapshot, QString *) {
            ++writes;
            current = snapshot;
            return true;
        });
    QSignalSpy changed(&recovery, &WorkspaceRecovery::workspaceChanged);

    QVERIFY(!recovery.importWorkspace(QUrl::fromLocalFile(importPath)));
    QCOMPARE(writes, 0);
    QVERIFY(recovery.lastError().contains(QStringLiteral("先预览")));
    QVERIFY(recovery.previewImport(QUrl::fromLocalFile(importPath))
                .value(QStringLiteral("valid"))
                .toBool());
    QVERIFY2(recovery.importWorkspace(QUrl::fromLocalFile(importPath)),
             qPrintable(recovery.lastError()));
    QCOMPARE(writes, 1);
    QCOMPARE(current.notes.front().id, 2);
    QCOMPARE(changed.count(), 1);
    const QFileInfoList safety =
        backupFiles(recovery.backupDirectory(),
                    QStringLiteral("before-danger-*.chrononotes"));
    QCOMPARE(safety.size(), 1);
    BackupService::WorkspaceSnapshot savedOriginal;
    QString error;
    QVERIFY2(BackupService::importWorkspaceV2(
                 QUrl::fromLocalFile(safety.front().absoluteFilePath()),
                 &savedOriginal, nullptr, &error),
             qPrintable(error));
    QCOMPARE(savedOriginal.notes.front().id, 1);
}

void WorkspaceRecoveryTests::failedImportRollsBackOriginalSnapshot() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const BackupService::WorkspaceSnapshot original =
        snapshotWithNote(1, QStringLiteral("Original"));
    BackupService::WorkspaceSnapshot current = original;
    const BackupService::WorkspaceSnapshot imported =
        snapshotWithNote(2, QStringLiteral("Imported"));
    const QString importPath = directory.filePath(QStringLiteral("incoming.chrononotes"));
    QVERIFY(exportSnapshot(importPath, imported));

    int writes = 0;
    WorkspaceRecovery recovery(directory.filePath(QStringLiteral("backups")),
                               QStringLiteral("1.0.0"));
    recovery.setSnapshotAccessors(
        [&](BackupService::WorkspaceSnapshot *snapshot, QString *) {
            *snapshot = current;
            return true;
        },
        [&](const BackupService::WorkspaceSnapshot &snapshot, QString *error) {
            ++writes;
            current = snapshot;  // Simulate a partially-applied first attempt.
            if (writes == 1) {
                if (error != nullptr) {
                    *error = QStringLiteral("simulated write failure");
                }
                return false;
            }
            return true;
        });

    QVERIFY(recovery.previewImport(QUrl::fromLocalFile(importPath))
                .value(QStringLiteral("valid"))
                .toBool());
    QVERIFY(!recovery.restoreBackup(QUrl::fromLocalFile(importPath)));
    QCOMPARE(writes, 2);
    QCOMPARE(current.notes.front().id, original.notes.front().id);
    QVERIFY(recovery.lastError().contains(QStringLiteral("已恢复")));
}

void WorkspaceRecoveryTests::previewRejectsChangedSourceOrCurrentWorkspace() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupService::WorkspaceSnapshot current =
        snapshotWithNote(1, QStringLiteral("Original"));
    const QString importPath = directory.filePath(QStringLiteral("incoming.chrononotes"));
    QVERIFY(exportSnapshot(importPath, snapshotWithNote(2, QStringLiteral("Imported"))));

    int writes = 0;
    WorkspaceRecovery recovery(directory.filePath(QStringLiteral("backups")),
                               QStringLiteral("1.0.0"));
    recovery.setSnapshotAccessors(
        [&](BackupService::WorkspaceSnapshot *snapshot, QString *) {
            *snapshot = current;
            return true;
        },
        [&](const BackupService::WorkspaceSnapshot &, QString *) {
            ++writes;
            return true;
        });

    QVERIFY(recovery.previewImport(QUrl::fromLocalFile(importPath))
                .value(QStringLiteral("valid"))
                .toBool());
    QVERIFY(exportSnapshot(importPath, snapshotWithNote(3, QStringLiteral("Changed source"))));
    QVERIFY(!recovery.importWorkspace(QUrl::fromLocalFile(importPath)));
    QVERIFY(recovery.lastError().contains(QStringLiteral("文件在预览后发生变化")));
    QCOMPARE(writes, 0);

    QVERIFY(recovery.previewImport(QUrl::fromLocalFile(importPath))
                .value(QStringLiteral("valid"))
                .toBool());
    current = snapshotWithNote(4, QStringLiteral("Changed current workspace"));
    QVERIFY(!recovery.importWorkspace(QUrl::fromLocalFile(importPath)));
    QVERIFY(recovery.lastError().contains(QStringLiteral("当前工作区在预览后发生变化")));
    QCOMPARE(writes, 0);
    QCOMPARE(backupFiles(recovery.backupDirectory(),
                         QStringLiteral("before-danger-*.chrononotes")).size(),
             0);
}

void WorkspaceRecoveryTests::keepsSevenDailyAndThreeDangerousBackups() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupService::WorkspaceSnapshot current =
        snapshotWithNote(1, QStringLiteral("Current"));
    QDateTime now =
        QDateTime::fromString(QStringLiteral("2026-07-01T09:00:00Z"), Qt::ISODate);
    WorkspaceRecovery recovery(directory.filePath(QStringLiteral("backups")),
                               QStringLiteral("1.0.0"));
    recovery.setClockForTesting([&] { return now; });
    recovery.setSnapshotAccessors(
        [&](BackupService::WorkspaceSnapshot *snapshot, QString *) {
            *snapshot = current;
            return true;
        },
        [&](const BackupService::WorkspaceSnapshot &snapshot, QString *) {
            current = snapshot;
            return true;
        });

    for (int index = 0; index < 9; ++index) {
        QVERIFY2(!recovery.ensureDailyBackup().isEmpty(),
                 qPrintable(recovery.lastError()));
        now = now.addDays(1);
    }
    QCOMPARE(backupFiles(recovery.backupDirectory(),
                         QStringLiteral("daily-*.chrononotes")).size(),
             7);

    const QString importPath = directory.filePath(QStringLiteral("incoming.chrononotes"));
    QVERIFY(exportSnapshot(importPath, snapshotWithNote(2, QStringLiteral("Imported"))));
    for (int index = 0; index < 5; ++index) {
        now = now.addSecs(1);
        QVERIFY(recovery.previewImport(QUrl::fromLocalFile(importPath))
                    .value(QStringLiteral("valid"))
                    .toBool());
        QVERIFY2(recovery.importWorkspace(QUrl::fromLocalFile(importPath)),
                 qPrintable(recovery.lastError()));
    }
    QCOMPARE(backupFiles(recovery.backupDirectory(),
                         QStringLiteral("before-danger-*.chrononotes")).size(),
             3);
}

void WorkspaceRecoveryTests::enforcesTwoHundredMiBManagedBackupLimit() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString backupsPath = directory.filePath(QStringLiteral("backups"));
    QVERIFY(QDir().mkpath(backupsPath));
    for (int index = 0; index < 5; ++index) {
        QFile file(QDir(backupsPath).filePath(
            QStringLiteral("manual-202607%1-090000-000.chrononotes")
                .arg(index + 1, 2, 10, QLatin1Char('0'))));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(45LL * 1024LL * 1024LL));
        QVERIFY(file.setFileTime(
            QDateTime::fromString(QStringLiteral("2026-07-%1T09:00:00Z")
                                      .arg(index + 1, 2, 10, QLatin1Char('0')),
                                  Qt::ISODate),
            QFileDevice::FileModificationTime));
        file.close();
    }

    BackupService::WorkspaceSnapshot current =
        snapshotWithNote(1, QStringLiteral("Current"));
    WorkspaceRecovery recovery(backupsPath, QStringLiteral("1.0.0"));
    recovery.setSnapshotAccessors(
        [&](BackupService::WorkspaceSnapshot *snapshot, QString *) {
            *snapshot = current;
            return true;
        },
        [&](const BackupService::WorkspaceSnapshot &, QString *) { return true; });

    QVERIFY2(!recovery.createBackup().isEmpty(), qPrintable(recovery.lastError()));
    qint64 total = 0;
    for (const QFileInfo &file :
         backupFiles(backupsPath, QStringLiteral("*.chrononotes"))) {
        total += file.size();
    }
    QVERIFY(total <= 200LL * 1024LL * 1024LL);
}

void WorkspaceRecoveryTests::diagnosticsAreStructuredAndStrictlyRedacted() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WorkspaceRecovery recovery(directory.filePath(QStringLiteral("backups")),
                               QStringLiteral("1.0.0"));
    recovery.setClockForTesting([] {
        return QDateTime::fromString(QStringLiteral("2026-07-26T09:30:00Z"),
                                     Qt::ISODate);
    });
    recovery.setDiagnosticsProvider([] {
        return QJsonObject{
            {QStringLiteral("schemaVersion"), 2},
            {QStringLiteral("noteCount"), 42},
            {QStringLiteral("databaseState"), QStringLiteral("locked")},
            {QStringLiteral("databaseErrorCode"), QStringLiteral("SQLITE_BUSY")},
            {QStringLiteral("portableMode"), true},
            {QStringLiteral("apiKey"), QStringLiteral("sk-secret")},
            {QStringLiteral("noteBody"), QStringLiteral("private note text")},
            {QStringLiteral("projectDescription"), QStringLiteral("private project")},
            {QStringLiteral("freeFormError"), QStringLiteral("Bearer token-value")},
        };
    });
    const QString path = directory.filePath(QStringLiteral("diagnostics.json"));
    QCOMPARE(recovery.exportDiagnostics(QUrl::fromLocalFile(path)), path);

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    QVERIFY(bytes.size() < 50LL * 1024LL * 1024LL);
    QVERIFY(!bytes.contains("sk-secret"));
    QVERIFY(!bytes.contains("private note text"));
    QVERIFY(!bytes.contains("private project"));
    QVERIFY(!bytes.contains("Bearer"));
    const QJsonObject runtime =
        QJsonDocument::fromJson(bytes).object().value(QStringLiteral("runtime")).toObject();
    QCOMPARE(runtime.value(QStringLiteral("schemaVersion")).toInt(), 2);
    QCOMPARE(runtime.value(QStringLiteral("noteCount")).toInt(), 42);
    QCOMPARE(runtime.value(QStringLiteral("databaseState")).toString(),
             QStringLiteral("locked"));
    QCOMPARE(runtime.value(QStringLiteral("databaseErrorCode")).toString(),
             QStringLiteral("sqlite_busy"));
    QVERIFY(runtime.value(QStringLiteral("portableMode")).toBool());
}

QTEST_MAIN(WorkspaceRecoveryTests)

#include "workspace_recovery_tests.moc"
