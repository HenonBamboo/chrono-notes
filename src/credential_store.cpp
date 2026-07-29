#include "credential_store.h"

#include <QCryptographicHash>
#include <QDir>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace {

QMutex &testMutex() {
    static QMutex mutex;
    return mutex;
}

QHash<QString, QString> &testSecrets() {
    static QHash<QString, QString> values;
    return values;
}

void setError(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
}

QString windowsErrorMessage(const QString &action) {
#ifdef Q_OS_WIN
    return QStringLiteral("%1失败，Windows 错误码：%2。").arg(action).arg(GetLastError());
#else
    return QStringLiteral("%1失败：当前平台不支持 Windows 凭据库。").arg(action);
#endif
}

} // namespace

CredentialStore::CredentialStore(const QString &dataDirectory, bool testMode)
    : testMode_(testMode) {
    const QByteArray digest = QCryptographicHash::hash(
        QDir::cleanPath(dataDirectory).toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    targetName_ = QStringLiteral("ChronoNotes/API/%1").arg(QString::fromLatin1(digest));
}

bool CredentialStore::hasSecret() const {
    QString ignored;
    return readSecret(&ignored, nullptr) && !ignored.isEmpty();
}

bool CredentialStore::readSecret(QString *secret, QString *error) const {
    if (secret == nullptr) {
        setError(error, QStringLiteral("凭据读取目标无效。"));
        return false;
    }
    secret->clear();
    if (testMode_) {
        QMutexLocker locker(&testMutex());
        if (!testSecrets().contains(targetName_)) {
            return false;
        }
        *secret = testSecrets().value(targetName_);
        return true;
    }

#ifdef Q_OS_WIN
    PCREDENTIALW credential = nullptr;
    const std::wstring target = targetName_.toStdWString();
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        if (GetLastError() != ERROR_NOT_FOUND) {
            setError(error, windowsErrorMessage(QStringLiteral("读取 API 凭据")));
        }
        return false;
    }
    const QByteArray bytes(reinterpret_cast<const char *>(credential->CredentialBlob),
                           static_cast<qsizetype>(credential->CredentialBlobSize));
    *secret = QString::fromUtf8(bytes);
    CredFree(credential);
    return true;
#else
    setError(error, windowsErrorMessage(QStringLiteral("读取 API 凭据")));
    return false;
#endif
}

bool CredentialStore::writeSecret(const QString &secret, QString *error) {
    const QString trimmed = secret.trimmed();
    if (trimmed.isEmpty()) {
        return clearSecret(error);
    }
    const QByteArray bytes = trimmed.toUtf8();
#ifdef Q_OS_WIN
    constexpr qsizetype maximumCredentialBytes =
        CRED_MAX_CREDENTIAL_BLOB_SIZE;
#else
    constexpr qsizetype maximumCredentialBytes = 2560;
#endif
    if (bytes.size() > maximumCredentialBytes) {
        setError(error, QStringLiteral(
            "API Key 过长，无法写入 Windows 凭据库。"));
        return false;
    }
    if (testMode_) {
        QMutexLocker locker(&testMutex());
        testSecrets().insert(targetName_, trimmed);
        return true;
    }

#ifdef Q_OS_WIN
    const std::wstring target = targetName_.toStdWString();
    const std::wstring user = QStringLiteral("ChronoNotes").toStdWString();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t *>(target.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(bytes.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(bytes.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<wchar_t *>(user.c_str());
    if (!CredWriteW(&credential, 0)) {
        setError(error, windowsErrorMessage(QStringLiteral("保存 API 凭据")));
        return false;
    }
    return true;
#else
    setError(error, windowsErrorMessage(QStringLiteral("保存 API 凭据")));
    return false;
#endif
}

bool CredentialStore::clearSecret(QString *error) {
    if (testMode_) {
        QMutexLocker locker(&testMutex());
        testSecrets().remove(targetName_);
        return true;
    }

#ifdef Q_OS_WIN
    const std::wstring target = targetName_.toStdWString();
    if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND) {
        setError(error, windowsErrorMessage(QStringLiteral("删除 API 凭据")));
        return false;
    }
    return true;
#else
    setError(error, windowsErrorMessage(QStringLiteral("删除 API 凭据")));
    return false;
#endif
}
