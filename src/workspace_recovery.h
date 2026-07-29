#ifndef WORKSPACE_RECOVERY_H
#define WORKSPACE_RECOVERY_H

#include <QDateTime>
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

#include "backup_service.h"

class WorkspaceRecovery final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString backupDirectory READ backupDirectory CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)
    Q_PROPERTY(QVariantList backups READ availableBackups NOTIFY backupsChanged)

public:
    using SnapshotReader =
        std::function<bool(BackupService::WorkspaceSnapshot *snapshot, QString *error)>;
    using SnapshotWriter =
        std::function<bool(const BackupService::WorkspaceSnapshot &snapshot, QString *error)>;
    using DiagnosticsProvider = std::function<QJsonObject()>;
    using Clock = std::function<QDateTime()>;

    explicit WorkspaceRecovery(QString backupDirectory, QString appVersion,
                               QObject *parent = nullptr);

    QString backupDirectory() const;
    QString lastError() const;
    QVariantList availableBackups() const;

    // The writer owns the durable, cross-subsystem transaction. Returning true
    // means notes, projects, summaries and preferences were committed together.
    // If it returns false after a partial change, WorkspaceRecovery immediately
    // invokes it again with the original snapshot to perform the inverse commit.
    void setSnapshotAccessors(SnapshotReader reader, SnapshotWriter writer);
    void setDiagnosticsProvider(DiagnosticsProvider provider);
    void setClockForTesting(Clock clock);

    Q_INVOKABLE QString createBackup();
    Q_INVOKABLE QString ensureDailyBackup();
    Q_INVOKABLE QVariantMap previewImport(const QUrl &fileUrl);
    Q_INVOKABLE bool importWorkspace(const QUrl &fileUrl);
    Q_INVOKABLE bool restoreBackup(const QUrl &fileUrl);
    Q_INVOKABLE QString exportDiagnostics(const QUrl &fileUrl);
    Q_INVOKABLE void refreshBackups();

signals:
    void errorChanged();
    void backupsChanged();
    void workspaceChanged();
    void operationSucceeded(const QString &message, const QString &path);
    void operationFailed(const QString &message);

private:
    enum class BackupKind {
        Manual,
        Daily,
        BeforeDangerousOperation,
    };

    QString createBackupFromSnapshot(const BackupService::WorkspaceSnapshot &source,
                                     BackupKind kind);
    bool applyWorkspaceFile(const QUrl &fileUrl, const QString &operation);
    bool loadWorkspaceFile(const QUrl &fileUrl,
                           BackupService::WorkspaceSnapshot *snapshot,
                           QString *path, QByteArray *digest = nullptr);
    bool readCurrentSnapshot(BackupService::WorkspaceSnapshot *snapshot);
    QByteArray snapshotDigest(const BackupService::WorkspaceSnapshot &snapshot,
                              QString *error) const;
    void clearPreview();
    bool pruneBackups(const QString &protectedPath = {});
    QString uniqueBackupPath(BackupKind kind) const;
    QDateTime nowUtc() const;
    void setLastError(const QString &message);
    void clearLastError();

    QString backup_directory_;
    QString app_version_;
    QString last_error_;
    SnapshotReader snapshot_reader_;
    SnapshotWriter snapshot_writer_;
    DiagnosticsProvider diagnostics_provider_;
    Clock clock_;
    QString preview_path_;
    QByteArray preview_file_digest_;
    QByteArray preview_current_digest_;
    bool preview_valid_{false};
};

#endif
