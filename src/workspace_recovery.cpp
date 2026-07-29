#include "workspace_recovery.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QSysInfo>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr qint64 kMaximumManagedBackupBytes = 200LL * 1024LL * 1024LL;
constexpr qint64 kMaximumDiagnosticsBytes = 50LL * 1024LL * 1024LL;
constexpr qint64 kMaximumWorkspaceBytes = 50LL * 1024LL * 1024LL;
constexpr int kDailyBackupLimit = 7;
constexpr int kDangerousBackupLimit = 3;

QString backupCategory(const QString &name) {
    if (name.startsWith(QStringLiteral("daily-"))) {
        return QStringLiteral("daily");
    }
    if (name.startsWith(QStringLiteral("before-danger-"))) {
        return QStringLiteral("before-danger");
    }
    return QStringLiteral("manual");
}

int projectCount(const BackupService::WorkspaceSnapshot &snapshot) {
    return snapshot.projects.value(QStringLiteral("nodes")).toArray().size();
}

bool ensureDirectory(const QString &path, QString *error) {
    if (!path.trimmed().isEmpty() && (QDir(path).exists() || QDir().mkpath(path))) {
        return true;
    }
    if (error != nullptr) {
        *error = QStringLiteral("无法创建备份目录。");
    }
    return false;
}

bool writeAtomic(const QString &path, const QByteArray &bytes, QString *error) {
    const QString parentPath = QFileInfo(path).absolutePath();
    if (!ensureDirectory(parentPath, error)) {
        return false;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法写入诊断报告：%1").arg(file.errorString());
        }
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        if (error != nullptr) {
            *error = QStringLiteral("诊断报告写入不完整：%1").arg(file.errorString());
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("无法原子提交诊断报告：%1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

QJsonObject sanitizedDiagnostics(const QJsonObject &source) {
    // Diagnostics are deliberately structured and allow-listed. Free-form text,
    // note bodies, project descriptions, AI output, credentials and paths never
    // enter the report.
    static const QSet<QString> integerKeys{
        QStringLiteral("schemaVersion"),
        QStringLiteral("noteCount"),
        QStringLiteral("projectCount"),
        QStringLiteral("backupCount"),
    };
    static const QSet<QString> booleanKeys{
        QStringLiteral("portableMode"),
        QStringLiteral("reduceMotion"),
        QStringLiteral("localHttpEnabled"),
    };
    static const QSet<QString> stateKeys{
        QStringLiteral("databaseState"),
        QStringLiteral("buildType"),
        QStringLiteral("databaseErrorCode"),
    };
    static const QSet<QString> allowedDatabaseStates{
        QStringLiteral("ok"),
        QStringLiteral("read-only"),
        QStringLiteral("locked"),
        QStringLiteral("corrupt"),
        QStringLiteral("unavailable"),
        QStringLiteral("unknown"),
    };
    static const QSet<QString> allowedBuildTypes{
        QStringLiteral("debug"),
        QStringLiteral("release"),
        QStringLiteral("relwithdebinfo"),
        QStringLiteral("minsizerel"),
        QStringLiteral("unknown"),
    };

    QJsonObject sanitized;
    for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
        if (integerKeys.contains(it.key()) && it.value().isDouble()) {
            const double value = it.value().toDouble();
            if (value >= 0.0 && value <= 1000000000.0
                && std::isfinite(value) && std::floor(value) == value) {
                sanitized.insert(it.key(), value);
            }
            continue;
        }
        if (booleanKeys.contains(it.key()) && it.value().isBool()) {
            sanitized.insert(it.key(), it.value());
            continue;
        }
        if (!stateKeys.contains(it.key()) || !it.value().isString()) {
            continue;
        }
        const QString value = it.value().toString().trimmed().toLower();
        if (it.key() == QStringLiteral("databaseState")
            && allowedDatabaseStates.contains(value)) {
            sanitized.insert(it.key(), value);
        } else if (it.key() == QStringLiteral("buildType")
                   && allowedBuildTypes.contains(value)) {
            sanitized.insert(it.key(), value);
        } else if (it.key() == QStringLiteral("databaseErrorCode")
                   && value.size() <= 64) {
            bool safe = true;
            for (const QChar ch : value) {
                if (!ch.isLetterOrNumber() && ch != QLatin1Char('-')
                    && ch != QLatin1Char('_') && ch != QLatin1Char('.')) {
                    safe = false;
                    break;
                }
            }
            if (safe) {
                sanitized.insert(it.key(), value);
            }
        }
    }
    return sanitized;
}

}  // namespace

WorkspaceRecovery::WorkspaceRecovery(QString backupDirectory, QString appVersion,
                                     QObject *parent)
    : QObject(parent),
      backup_directory_(backupDirectory.trimmed().isEmpty()
                            ? QString{}
                            : QDir::cleanPath(std::move(backupDirectory))),
      app_version_(std::move(appVersion)),
      clock_([] { return QDateTime::currentDateTimeUtc(); }) {
}

QString WorkspaceRecovery::backupDirectory() const {
    return backup_directory_;
}

QString WorkspaceRecovery::lastError() const {
    return last_error_;
}

QVariantList WorkspaceRecovery::availableBackups() const {
    QVariantList result;
    const QDir directory(backup_directory_);
    const QFileInfoList files = directory.entryInfoList(
        QStringList{QStringLiteral("*.chrononotes")}, QDir::Files, QDir::Time);
    result.reserve(files.size());
    for (const QFileInfo &file : files) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), file.fileName());
        item.insert(QStringLiteral("path"), file.absoluteFilePath());
        item.insert(QStringLiteral("url"), QUrl::fromLocalFile(file.absoluteFilePath()));
        item.insert(QStringLiteral("category"), backupCategory(file.fileName()));
        item.insert(QStringLiteral("size"), file.size());
        item.insert(QStringLiteral("createdAt"), file.lastModified().toUTC());
        result.append(item);
    }
    return result;
}

void WorkspaceRecovery::setSnapshotAccessors(SnapshotReader reader, SnapshotWriter writer) {
    clearPreview();
    snapshot_reader_ = std::move(reader);
    snapshot_writer_ = std::move(writer);
}

void WorkspaceRecovery::setDiagnosticsProvider(DiagnosticsProvider provider) {
    diagnostics_provider_ = std::move(provider);
}

void WorkspaceRecovery::setClockForTesting(Clock clock) {
    clock_ = clock ? std::move(clock)
                   : Clock([] { return QDateTime::currentDateTimeUtc(); });
}

QString WorkspaceRecovery::createBackup() {
    clearLastError();
    BackupService::WorkspaceSnapshot snapshot;
    if (!readCurrentSnapshot(&snapshot)) {
        emit operationFailed(last_error_);
        return {};
    }
    const QString path = createBackupFromSnapshot(snapshot, BackupKind::Manual);
    if (path.isEmpty()) {
        emit operationFailed(last_error_);
        return {};
    }
    emit operationSucceeded(QStringLiteral("工作区备份已创建。"), path);
    return path;
}

QString WorkspaceRecovery::ensureDailyBackup() {
    clearLastError();
    QString error;
    if (!ensureDirectory(backup_directory_, &error)) {
        setLastError(error);
        emit operationFailed(last_error_);
        return {};
    }
    const QString day = nowUtc().toString(QStringLiteral("yyyyMMdd"));
    const QFileInfoList existing = QDir(backup_directory_).entryInfoList(
        QStringList{QStringLiteral("daily-%1-*.chrononotes").arg(day)},
        QDir::Files, QDir::Time);
    for (const QFileInfo &candidate : existing) {
        BackupService::WorkspaceSnapshot verification;
        QString verificationError;
        if (!BackupService::importWorkspaceV2(
                QUrl::fromLocalFile(candidate.absoluteFilePath()),
                &verification, nullptr, &verificationError)) {
            continue;
        }
        if (!pruneBackups(candidate.absoluteFilePath())) {
            emit operationFailed(last_error_);
            return {};
        }
        return candidate.absoluteFilePath();
    }

    BackupService::WorkspaceSnapshot snapshot;
    if (!readCurrentSnapshot(&snapshot)) {
        emit operationFailed(last_error_);
        return {};
    }
    const QString path = createBackupFromSnapshot(snapshot, BackupKind::Daily);
    if (path.isEmpty()) {
        emit operationFailed(last_error_);
        return {};
    }
    emit operationSucceeded(QStringLiteral("每日自动备份已创建。"), path);
    return path;
}

QVariantMap WorkspaceRecovery::previewImport(const QUrl &fileUrl) {
    clearLastError();
    clearPreview();
    QVariantMap result;
    result.insert(QStringLiteral("valid"), false);

    BackupService::WorkspaceSnapshot imported;
    QString path;
    QByteArray fileDigest;
    if (!loadWorkspaceFile(fileUrl, &imported, &path, &fileDigest)) {
        result.insert(QStringLiteral("error"), last_error_);
        emit operationFailed(last_error_);
        return result;
    }

    BackupService::WorkspaceSnapshot current;
    if (!readCurrentSnapshot(&current)) {
        result.insert(QStringLiteral("error"), last_error_);
        emit operationFailed(last_error_);
        return result;
    }
    QString digestError;
    const QByteArray currentDigest = snapshotDigest(current, &digestError);
    if (currentDigest.isEmpty()) {
        setLastError(QStringLiteral("无法校验当前工作区：%1").arg(digestError));
        result.insert(QStringLiteral("error"), last_error_);
        emit operationFailed(last_error_);
        return result;
    }
    preview_path_ = QFileInfo(path).canonicalFilePath();
    if (preview_path_.isEmpty()) {
        preview_path_ = QFileInfo(path).absoluteFilePath();
    }
    preview_file_digest_ = fileDigest;
    preview_current_digest_ = currentDigest;
    preview_valid_ = true;

    result.insert(QStringLiteral("valid"), true);
    result.insert(QStringLiteral("path"), path);
    result.insert(QStringLiteral("createdAt"), imported.createdAt);
    result.insert(QStringLiteral("appVersion"), imported.appVersion);
    result.insert(QStringLiteral("noteCount"), imported.notes.size());
    result.insert(QStringLiteral("currentNoteCount"), current.notes.size());
    result.insert(QStringLiteral("noteDelta"), imported.notes.size() - current.notes.size());
    result.insert(QStringLiteral("projectCount"), projectCount(imported));
    result.insert(QStringLiteral("currentProjectCount"), projectCount(current));
    result.insert(QStringLiteral("projectDelta"), projectCount(imported) - projectCount(current));
    result.insert(QStringLiteral("summaryCount"), imported.summaries.size());
    result.insert(QStringLiteral("currentSummaryCount"), current.summaries.size());
    result.insert(QStringLiteral("willReplace"), true);
    return result;
}

bool WorkspaceRecovery::importWorkspace(const QUrl &fileUrl) {
    return applyWorkspaceFile(fileUrl, QStringLiteral("导入"));
}

bool WorkspaceRecovery::restoreBackup(const QUrl &fileUrl) {
    return applyWorkspaceFile(fileUrl, QStringLiteral("恢复"));
}

QString WorkspaceRecovery::exportDiagnostics(const QUrl &fileUrl) {
    clearLastError();
    const QString path = BackupService::localPathFromUrl(fileUrl);
    if (path.trimmed().isEmpty()) {
        setLastError(QStringLiteral("请选择诊断报告导出位置。"));
        emit operationFailed(last_error_);
        return {};
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("chrononotes-diagnostics"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("createdAt"), nowUtc().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("appVersion"), app_version_);
    root.insert(QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()));
    QJsonObject platform;
    platform.insert(QStringLiteral("productType"), QSysInfo::productType());
    platform.insert(QStringLiteral("productVersion"), QSysInfo::productVersion());
    platform.insert(QStringLiteral("kernelType"), QSysInfo::kernelType());
    platform.insert(QStringLiteral("kernelVersion"), QSysInfo::kernelVersion());
    platform.insert(QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture());
    root.insert(QStringLiteral("platform"), platform);

    QJsonObject backupState;
    const QVariantList backups = availableBackups();
    qint64 backupBytes = 0;
    for (const QVariant &item : backups) {
        backupBytes += item.toMap().value(QStringLiteral("size")).toLongLong();
    }
    backupState.insert(QStringLiteral("count"), backups.size());
    backupState.insert(QStringLiteral("bytes"), QString::number(backupBytes));
    root.insert(QStringLiteral("backups"), backupState);
    root.insert(QStringLiteral("runtime"),
                sanitizedDiagnostics(diagnostics_provider_ ? diagnostics_provider_()
                                                          : QJsonObject{}));

    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (bytes.size() > kMaximumDiagnosticsBytes) {
        setLastError(QStringLiteral("诊断报告超过 50 MiB 安全上限。"));
        emit operationFailed(last_error_);
        return {};
    }
    QString error;
    if (!writeAtomic(path, bytes, &error)) {
        setLastError(error);
        emit operationFailed(last_error_);
        return {};
    }
    emit operationSucceeded(QStringLiteral("脱敏诊断报告已导出。"), path);
    return path;
}

void WorkspaceRecovery::refreshBackups() {
    emit backupsChanged();
}

QString WorkspaceRecovery::createBackupFromSnapshot(
    const BackupService::WorkspaceSnapshot &source, BackupKind kind) {
    QString error;
    if (!ensureDirectory(backup_directory_, &error)) {
        setLastError(error);
        return {};
    }

    BackupService::WorkspaceSnapshot snapshot = source;
    snapshot.createdAt = nowUtc().toString(Qt::ISODateWithMs);
    snapshot.appVersion = app_version_;
    const QString path = uniqueBackupPath(kind);
    if (!BackupService::exportWorkspaceV2(snapshot, QUrl::fromLocalFile(path),
                                          nullptr, &error)) {
        setLastError(error);
        return {};
    }

    BackupService::WorkspaceSnapshot verification;
    if (!BackupService::importWorkspaceV2(QUrl::fromLocalFile(path), &verification,
                                          nullptr, &error)) {
        QFile::remove(path);
        setLastError(QStringLiteral("备份写入后校验失败：%1").arg(error));
        return {};
    }
    if (!pruneBackups(path)) {
        QFile::remove(path);
        emit backupsChanged();
        return {};
    }
    emit backupsChanged();
    return path;
}

bool WorkspaceRecovery::applyWorkspaceFile(const QUrl &fileUrl,
                                           const QString &operation) {
    clearLastError();
    const QString requestedPath = BackupService::localPathFromUrl(fileUrl);
    QString canonicalPath = QFileInfo(requestedPath).canonicalFilePath();
    if (canonicalPath.isEmpty()) {
        canonicalPath = QFileInfo(requestedPath).absoluteFilePath();
    }
    if (!preview_valid_ || canonicalPath != preview_path_) {
        setLastError(QStringLiteral("请先预览并确认工作区影响，再执行%1。").arg(operation));
        emit operationFailed(last_error_);
        return false;
    }

    BackupService::WorkspaceSnapshot imported;
    QString sourcePath;
    QByteArray currentFileDigest;
    if (!loadWorkspaceFile(fileUrl, &imported, &sourcePath, &currentFileDigest)) {
        clearPreview();
        emit operationFailed(last_error_);
        return false;
    }
    if (currentFileDigest != preview_file_digest_) {
        clearPreview();
        setLastError(QStringLiteral("工作区文件在预览后发生变化，请重新预览。"));
        emit operationFailed(last_error_);
        return false;
    }

    BackupService::WorkspaceSnapshot current;
    if (!readCurrentSnapshot(&current)) {
        clearPreview();
        emit operationFailed(last_error_);
        return false;
    }
    QString digestError;
    const QByteArray currentDigest = snapshotDigest(current, &digestError);
    if (currentDigest.isEmpty() || currentDigest != preview_current_digest_) {
        clearPreview();
        setLastError(currentDigest.isEmpty()
                         ? QStringLiteral("无法校验当前工作区：%1").arg(digestError)
                         : QStringLiteral("当前工作区在预览后发生变化，请重新预览。"));
        emit operationFailed(last_error_);
        return false;
    }
    if (!snapshot_writer_) {
        clearPreview();
        setLastError(QStringLiteral("工作区写入器未配置。"));
        emit operationFailed(last_error_);
        return false;
    }

    const QString safetyBackup =
        createBackupFromSnapshot(current, BackupKind::BeforeDangerousOperation);
    if (safetyBackup.isEmpty()) {
        clearPreview();
        emit operationFailed(last_error_);
        return false;
    }

    QString writeError;
    if (!snapshot_writer_(imported, &writeError)) {
        QString rollbackError;
        if (snapshot_writer_(current, &rollbackError)) {
            setLastError(QStringLiteral("%1失败，原工作区已恢复：%2")
                             .arg(operation,
                                  writeError.isEmpty() ? QStringLiteral("未知写入错误")
                                                       : writeError));
        } else {
            setLastError(QStringLiteral("%1失败且自动回滚失败：%2；回滚错误：%3")
                             .arg(operation,
                                  writeError.isEmpty() ? QStringLiteral("未知写入错误")
                                                       : writeError,
                                  rollbackError.isEmpty() ? QStringLiteral("未知回滚错误")
                                                          : rollbackError));
        }
        clearPreview();
        emit operationFailed(last_error_);
        return false;
    }

    clearPreview();
    emit workspaceChanged();
    emit operationSucceeded(QStringLiteral("工作区%1完成。").arg(operation), sourcePath);
    return true;
}

bool WorkspaceRecovery::loadWorkspaceFile(
    const QUrl &fileUrl, BackupService::WorkspaceSnapshot *snapshot,
    QString *path, QByteArray *digest) {
    const QString resolvedPath = BackupService::localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        setLastError(QStringLiteral("请选择本地工作区备份文件。"));
        return false;
    }
    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(QStringLiteral("无法读取工作区备份：%1").arg(file.errorString()));
        return false;
    }
    if (file.size() < 0 || file.size() > kMaximumWorkspaceBytes) {
        setLastError(QStringLiteral("工作区备份超过 50 MiB 安全上限。"));
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError || bytes.size() != file.size()) {
        setLastError(QStringLiteral("工作区备份读取不完整：%1").arg(file.errorString()));
        return false;
    }
    QString error;
    if (!BackupService::decodeWorkspaceV2(bytes, snapshot, &error)) {
        setLastError(error);
        return false;
    }
    if (path != nullptr) {
        *path = resolvedPath;
    }
    if (digest != nullptr) {
        *digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    }
    return true;
}

bool WorkspaceRecovery::readCurrentSnapshot(
    BackupService::WorkspaceSnapshot *snapshot) {
    if (snapshot == nullptr) {
        setLastError(QStringLiteral("工作区快照目标不可用。"));
        return false;
    }
    if (!snapshot_reader_) {
        setLastError(QStringLiteral("工作区读取器未配置。"));
        return false;
    }
    QString error;
    if (!snapshot_reader_(snapshot, &error)) {
        setLastError(error.isEmpty() ? QStringLiteral("无法读取当前工作区。") : error);
        return false;
    }
    return true;
}

QByteArray WorkspaceRecovery::snapshotDigest(
    const BackupService::WorkspaceSnapshot &snapshot, QString *error) const {
    BackupService::WorkspaceSnapshot canonical = snapshot;
    canonical.createdAt = QStringLiteral("2000-01-01T00:00:00Z");
    canonical.appVersion =
        app_version_.trimmed().isEmpty() ? QStringLiteral("unknown") : app_version_;
    const QByteArray bytes = BackupService::encodeWorkspaceV2(canonical, error);
    return bytes.isEmpty()
        ? QByteArray{}
        : QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

void WorkspaceRecovery::clearPreview() {
    preview_path_.clear();
    preview_file_digest_.clear();
    preview_current_digest_.clear();
    preview_valid_ = false;
}

bool WorkspaceRecovery::pruneBackups(const QString &protectedPath) {
    QDir directory(backup_directory_);
    QFileInfoList files = directory.entryInfoList(
        QStringList{QStringLiteral("*.chrononotes")}, QDir::Files, QDir::Time);

    auto removeExcess = [&](const QString &prefix, int limit) {
        QSet<QString> keptPaths;
        const QString normalizedProtected = QDir::cleanPath(protectedPath);
        for (const QFileInfo &file : std::as_const(files)) {
            if (file.fileName().startsWith(prefix)
                && QDir::cleanPath(file.absoluteFilePath()) == normalizedProtected) {
                keptPaths.insert(normalizedProtected);
                break;
            }
        }
        for (const QFileInfo &file : std::as_const(files)) {
            if (!file.fileName().startsWith(prefix)) {
                continue;
            }
            const QString normalizedPath = QDir::cleanPath(file.absoluteFilePath());
            if (keptPaths.size() < limit) {
                keptPaths.insert(normalizedPath);
            }
            if (keptPaths.contains(normalizedPath)) {
                continue;
            }
            if (!QFile::remove(file.absoluteFilePath())) {
                setLastError(QStringLiteral("无法清理过期备份：%1").arg(file.fileName()));
                return false;
            }
        }
        return true;
    };

    if (!removeExcess(QStringLiteral("daily-"), kDailyBackupLimit)
        || !removeExcess(QStringLiteral("before-danger-"), kDangerousBackupLimit)) {
        return false;
    }

    files = directory.entryInfoList(QStringList{QStringLiteral("*.chrononotes")},
                                    QDir::Files, QDir::Name);
    std::sort(files.begin(), files.end(),
              [](const QFileInfo &left, const QFileInfo &right) {
                  if (left.lastModified() == right.lastModified()) {
                      return left.fileName() < right.fileName();
                  }
                  return left.lastModified() < right.lastModified();
              });
    qint64 totalBytes = 0;
    for (const QFileInfo &file : std::as_const(files)) {
        totalBytes += std::max<qint64>(0, file.size());
    }
    for (const QFileInfo &file : std::as_const(files)) {
        if (totalBytes <= kMaximumManagedBackupBytes) {
            break;
        }
        if (QDir::cleanPath(file.absoluteFilePath())
            == QDir::cleanPath(protectedPath)) {
            continue;
        }
        const qint64 size = std::max<qint64>(0, file.size());
        if (!QFile::remove(file.absoluteFilePath())) {
            setLastError(QStringLiteral("无法执行备份空间上限：%1").arg(file.fileName()));
            return false;
        }
        totalBytes -= size;
    }
    if (totalBytes > kMaximumManagedBackupBytes) {
        setLastError(QStringLiteral("备份总量超过 200 MiB，且无法安全清理。"));
        return false;
    }
    return true;
}

QString WorkspaceRecovery::uniqueBackupPath(BackupKind kind) const {
    QString prefix;
    switch (kind) {
    case BackupKind::Daily:
        prefix = QStringLiteral("daily");
        break;
    case BackupKind::BeforeDangerousOperation:
        prefix = QStringLiteral("before-danger");
        break;
    case BackupKind::Manual:
    default:
        prefix = QStringLiteral("manual");
        break;
    }
    const QString timestamp =
        nowUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    QDir directory(backup_directory_);
    QString path =
        directory.filePath(QStringLiteral("%1-%2.chrononotes").arg(prefix, timestamp));
    for (int suffix = 2; QFileInfo::exists(path); ++suffix) {
        path = directory.filePath(
            QStringLiteral("%1-%2-%3.chrononotes").arg(prefix, timestamp)
                .arg(suffix));
    }
    return path;
}

QDateTime WorkspaceRecovery::nowUtc() const {
    const QDateTime value = clock_ ? clock_() : QDateTime::currentDateTimeUtc();
    return value.isValid() ? value.toUTC() : QDateTime::currentDateTimeUtc();
}

void WorkspaceRecovery::setLastError(const QString &message) {
    if (last_error_ == message) {
        return;
    }
    last_error_ = message;
    emit errorChanged();
}

void WorkspaceRecovery::clearLastError() {
    setLastError({});
}
