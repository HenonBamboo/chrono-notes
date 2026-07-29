#include "local_profile.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QString cleanPath(const QString &path) {
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

void setError(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
}

QStringList migratableFileNames() {
    return {
        QStringLiteral("notes.sqlite"),
        QStringLiteral("notes.db.txt"),
        QStringLiteral("config.ini"),
        QStringLiteral("operations.jsonl"),
        QStringLiteral("summary-history.md"),
        QStringLiteral("project-tree.json")
    };
}

bool destinationHasUserData(const QString &dataDir) {
    for (const QString &name : migratableFileNames()) {
        if (QFileInfo::exists(QDir(dataDir).filePath(name))) {
            return true;
        }
    }
    return false;
}

} // namespace

LocalProfilePaths LocalProfile::resolve() {
    LocalProfilePaths paths;
    const QString applicationDir = cleanPath(QCoreApplication::applicationDirPath());
    const QString overrideDir = qEnvironmentVariable("STICKY_NOTES_DATA_DIR").trimmed();
    paths.overridden = !overrideDir.isEmpty();
    paths.portable = !paths.overridden && QFileInfo::exists(QDir(applicationDir).filePath(QStringLiteral("portable.flag")));

    if (paths.overridden) {
        paths.dataDir = cleanPath(overrideDir);
    } else if (paths.portable) {
        paths.dataDir = QDir(applicationDir).filePath(QStringLiteral("data"));
    } else {
        paths.dataDir = cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    }

    const QDir data(paths.dataDir);
    paths.legacyNotesPath = data.filePath(QStringLiteral("notes.db.txt"));
    paths.sqlitePath = data.filePath(QStringLiteral("notes.sqlite"));
    paths.configPath = data.filePath(QStringLiteral("config.ini"));
    paths.operationLogPath = data.filePath(QStringLiteral("operations.jsonl"));
    paths.summaryHistoryPath = data.filePath(QStringLiteral("summary-history.md"));
    paths.projectTreePath = data.filePath(QStringLiteral("project-tree.json"));
    paths.backupDir = data.filePath(QStringLiteral("Backups"));
    paths.diagnosticLogPath = data.filePath(QStringLiteral("diagnostics.log"));
    return paths;
}

bool LocalProfile::ensureAndMigrate(const LocalProfilePaths &paths, QString *error) {
    if (paths.dataDir.trimmed().isEmpty()) {
        setError(error, QStringLiteral("无法确定 ChronoNotes 用户数据目录。"));
        return false;
    }
    if (!QDir().mkpath(paths.dataDir) || !QDir().mkpath(paths.backupDir)) {
        setError(error, QStringLiteral("无法创建用户数据目录：%1").arg(QDir::toNativeSeparators(paths.dataDir)));
        return false;
    }

    if (paths.overridden || paths.portable || destinationHasUserData(paths.dataDir)) {
        return true;
    }

    const QString legacyDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data"));
    if (!QFileInfo(legacyDir).isDir() || cleanPath(legacyDir) == cleanPath(paths.dataDir)) {
        return true;
    }

    QStringList existing;
    for (const QString &name : migratableFileNames()) {
        if (QFileInfo::exists(QDir(legacyDir).filePath(name))) {
            existing.append(name);
        }
    }
    if (existing.isEmpty()) {
        return true;
    }

    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString migrationBackup = QDir(paths.backupDir).filePath(QStringLiteral("migration-%1").arg(stamp));
    if (!QDir().mkpath(migrationBackup)) {
        setError(error, QStringLiteral("无法创建迁移备份目录。"));
        return false;
    }

    for (const QString &name : existing) {
        const QString source = QDir(legacyDir).filePath(name);
        if (!copyFileAtomically(source, QDir(migrationBackup).filePath(name), error) ||
            !copyFileAtomically(source, QDir(paths.dataDir).filePath(name), error)) {
            return false;
        }
    }

    QSaveFile marker(QDir(paths.dataDir).filePath(QStringLiteral(".migration-v1")));
    marker.setDirectWriteFallback(false);
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Text) ||
        marker.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8()) < 0 ||
        !marker.commit()) {
        setError(error, QStringLiteral("数据已复制，但无法写入迁移完成标记。"));
        return false;
    }
    return true;
}

bool LocalProfile::copyFileAtomically(const QString &source, const QString &destination, QString *error) {
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("无法读取迁移源文件：%1").arg(QDir::toNativeSeparators(source)));
        return false;
    }
    if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
        setError(error, QStringLiteral("无法创建迁移目标目录。"));
        return false;
    }

    QSaveFile output(destination);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("无法创建迁移目标文件：%1").arg(QDir::toNativeSeparators(destination)));
        return false;
    }

    QByteArray chunk;
    chunk.resize(64 * 1024);
    while (!input.atEnd()) {
        const qint64 read = input.read(chunk.data(), chunk.size());
        if (read < 0 || output.write(chunk.constData(), read) != read) {
            output.cancelWriting();
            setError(error, QStringLiteral("迁移文件写入失败，原文件保持不变。"));
            return false;
        }
    }
    if (!output.commit()) {
        setError(error, QStringLiteral("迁移文件提交失败，原文件保持不变。"));
        return false;
    }
    return true;
}
