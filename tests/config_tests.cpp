#include "config.h"

#include <QFile>

#include <cstdio>

namespace {

int failures = 0;

void expectTrue(const char *name, bool value) {
    if (!value) {
        std::printf("FAIL %s\n", name);
        ++failures;
    }
}

void expectInt(const char *name, int expected, int actual) {
    if (expected != actual) {
        std::printf("FAIL %s: expected %d, got %d\n", name, expected, actual);
        ++failures;
    }
}

void expectString(const char *name, const QString &expected,
                  const QString &actual) {
    if (expected != actual) {
        std::printf("FAIL %s\n", name);
        ++failures;
    }
}

bool writeText(const QString &path, const QByteArray &contents) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(contents) == contents.size();
}

void testDefaultsAreUsableWithoutKey() {
    const AppConfig config = defaultConfig();
    expectString("default api url",
                 QStringLiteral("https://api.openai.com/v1/chat/completions"),
                 config.apiUrl);
    expectString("default model is explicit", QString(), config.model);
    expectString("default ui font family",
                 QStringLiteral("Microsoft YaHei UI"),
                 config.uiFontFamily);
    expectInt("default ui font size", 14, config.uiFontSize);
    expectTrue("default ai key is absent", config.legacyApiKey.isEmpty());
}

void testSaveAndLoadConfig() {
    const QString path = QStringLiteral("config_test.ini");
    AppConfig config = defaultConfig();
    config.apiUrl = QStringLiteral("https://example.test/v1/chat/completions");
    config.legacyApiKey = QStringLiteral("test-key");
    config.model = QStringLiteral("test-model");
    config.uiFontFamily = QStringLiteral("Segoe UI");
    config.uiFontSize = 15;
    config.allowLocalHttp = true;
    config.reduceMotion = true;

    QString error;
    expectTrue("config save succeeds", saveConfig(path, config, &error));
    AppConfig loaded;
    expectTrue("config load succeeds", loadConfig(path, &loaded, &error));
    expectString("loaded api url", config.apiUrl, loaded.apiUrl);
    expectTrue("api key is not persisted in settings",
               loaded.legacyApiKey.isEmpty());
    expectString("loaded model", config.model, loaded.model);
    expectString("loaded ui font family", config.uiFontFamily,
                 loaded.uiFontFamily);
    expectInt("loaded ui font size", config.uiFontSize, loaded.uiFontSize);
    expectTrue("loaded local http setting", loaded.allowLocalHttp);
    expectTrue("loaded reduce motion setting", loaded.reduceMotion);
    QFile::remove(path);
}

void testLegacyConfigUsesDefaultsAndExposesMigrationKey() {
    const QString path = QStringLiteral("config_legacy_test.ini");
    expectTrue(
        "legacy fixture writes",
        writeText(path,
                  QByteArrayLiteral(
                      "api_url=https://legacy.test/v1/chat/completions\n"
                      "api_key=legacy-key\n"
                      "model=legacy-model\n")));

    AppConfig loaded;
    QString error;
    expectTrue("legacy config load succeeds",
               loadConfig(path, &loaded, &error));
    expectString("legacy default ui font family",
                 QStringLiteral("Microsoft YaHei UI"),
                 loaded.uiFontFamily);
    expectInt("legacy default ui font size", 14, loaded.uiFontSize);
    expectString("legacy key available for credential migration",
                 QStringLiteral("legacy-key"), loaded.legacyApiKey);
    QFile::remove(path);
}

void testInvalidFontSizeFallsBackToDefault() {
    const QString path = QStringLiteral("config_invalid_font_size_test.ini");
    expectTrue("invalid font fixture writes",
               writeText(path,
                         QByteArrayLiteral("ui_font_family=Segoe UI\n"
                                           "ui_font_size=99\n")));

    AppConfig loaded;
    QString error;
    expectTrue("invalid font size config load succeeds",
               loadConfig(path, &loaded, &error));
    expectString("invalid font size keeps family",
                 QStringLiteral("Segoe UI"), loaded.uiFontFamily);
    expectInt("invalid font size defaults", 14, loaded.uiFontSize);
    QFile::remove(path);
}

void testFutureConfigIsRejectedWithoutMutation() {
    const QString path = QStringLiteral("config_future_test.ini");
    expectTrue("future config fixture writes",
               writeText(path,
                         QByteArrayLiteral("version=999\n"
                                           "model=future-model\n")));

    AppConfig loaded = defaultConfig();
    loaded.model = QStringLiteral("keep-me");
    QString error;
    expectTrue("future config is rejected",
               !loadConfig(path, &loaded, &error));
    expectString("rejected config does not mutate destination",
                 QStringLiteral("keep-me"), loaded.model);
    expectTrue("future config error is explicit",
               error.contains(QStringLiteral("更高版本")));
    QFile::remove(path);
}

} // namespace

int main() {
    testDefaultsAreUsableWithoutKey();
    testSaveAndLoadConfig();
    testLegacyConfigUsesDefaultsAndExposesMigrationKey();
    testInvalidFontSizeFallsBackToDefault();
    testFutureConfigIsRejectedWithoutMutation();

    if (failures != 0) {
        std::printf("%d config test(s) failed\n", failures);
        return 1;
    }
    std::printf("config tests passed\n");
    return 0;
}
