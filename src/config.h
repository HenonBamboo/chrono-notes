#ifndef CONFIG_H
#define CONFIG_H

#include <QString>

struct AppConfig {
    static constexpr int CurrentSchemaVersion = 2;
    static constexpr qsizetype MaximumUrlLength = 2048;
    static constexpr qsizetype MaximumModelLength = 256;
    static constexpr qsizetype MaximumFontFamilyLength = 256;
    static constexpr qsizetype MaximumLegacyKeyLength = 4096;

    QString apiUrl;
    QString legacyApiKey;
    QString model;
    QString uiFontFamily;
    int uiFontSize{14};
    bool allowLocalHttp{false};
    bool reduceMotion{false};

    bool operator==(const AppConfig &other) const {
        return apiUrl == other.apiUrl &&
               legacyApiKey == other.legacyApiKey &&
               model == other.model &&
               uiFontFamily == other.uiFontFamily &&
               uiFontSize == other.uiFontSize &&
               allowLocalHttp == other.allowLocalHttp &&
               reduceMotion == other.reduceMotion;
    }
};

AppConfig defaultConfig();
bool loadConfig(const QString &path, AppConfig *config, QString *error = nullptr);
bool saveConfig(const QString &path, const AppConfig &config, QString *error = nullptr);

#endif
