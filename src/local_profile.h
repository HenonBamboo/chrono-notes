#ifndef LOCAL_PROFILE_H
#define LOCAL_PROFILE_H

#include <QString>

struct LocalProfilePaths {
    QString dataDir;
    QString legacyNotesPath;
    QString sqlitePath;
    QString configPath;
    QString operationLogPath;
    QString summaryHistoryPath;
    QString projectTreePath;
    QString backupDir;
    QString diagnosticLogPath;
    bool portable{false};
    bool overridden{false};
};

class LocalProfile {
public:
    static LocalProfilePaths resolve();
    static bool ensureAndMigrate(const LocalProfilePaths &paths, QString *error = nullptr);

private:
    static bool copyFileAtomically(const QString &source, const QString &destination, QString *error);
};

#endif
