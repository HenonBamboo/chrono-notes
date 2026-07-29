#ifndef CREDENTIAL_STORE_H
#define CREDENTIAL_STORE_H

#include <QString>

class CredentialStore {
public:
    explicit CredentialStore(const QString &dataDirectory, bool testMode = false);

    bool hasSecret() const;
    bool readSecret(QString *secret, QString *error = nullptr) const;
    bool writeSecret(const QString &secret, QString *error = nullptr);
    bool clearSecret(QString *error = nullptr);

private:
    QString targetName_;
    bool testMode_{false};
};

#endif
