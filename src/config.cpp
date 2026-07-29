#include "config.h"

#include <QFile>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>

#include <utility>

namespace {

constexpr int kMinimumFontSize = 10;
constexpr int kMaximumFontSize = 18;
constexpr int kDefaultFontSize = 14;

void setError(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
}

int normalizedFontSize(int value) {
    return value >= kMinimumFontSize && value <= kMaximumFontSize
               ? value
               : kDefaultFontSize;
}

bool isSingleLineWithin(const QString &value, qsizetype maximumLength) {
    return value.size() <= maximumLength &&
           !value.contains(QLatin1Char('\n')) &&
           !value.contains(QLatin1Char('\r')) &&
           !value.contains(QChar::Null);
}

bool validateForSave(const AppConfig &config, QString *error) {
    if (!isSingleLineWithin(config.apiUrl, AppConfig::MaximumUrlLength)) {
        setError(error, QStringLiteral("AI 地址无效或超过 2,048 字符。"));
        return false;
    }
    if (!isSingleLineWithin(config.model, AppConfig::MaximumModelLength)) {
        setError(error, QStringLiteral("模型名称无效或超过 256 字符。"));
        return false;
    }
    if (!isSingleLineWithin(config.uiFontFamily, AppConfig::MaximumFontFamilyLength)) {
        setError(error, QStringLiteral("字体名称无效或超过 256 字符。"));
        return false;
    }
    if (config.uiFontSize < kMinimumFontSize ||
        config.uiFontSize > kMaximumFontSize) {
        setError(error, QStringLiteral("字体大小必须在 10 到 18 之间。"));
        return false;
    }
    return true;
}

} // namespace

AppConfig defaultConfig() {
    AppConfig config;
    config.apiUrl = QStringLiteral("https://api.openai.com/v1/chat/completions");
    config.uiFontFamily = QStringLiteral("Microsoft YaHei UI");
    return config;
}

bool loadConfig(const QString &path, AppConfig *config, QString *error) {
    if (config == nullptr || path.trimmed().isEmpty()) {
        setError(error, QStringLiteral("设置文件路径无效。"));
        return false;
    }

    AppConfig loaded = defaultConfig();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(error, QStringLiteral("设置文件无法读取：%1").arg(file.errorString()));
        return false;
    }

    int schemaVersion = 1;
    QTextStream input(&file);
    input.setEncoding(QStringConverter::Utf8);
    while (!input.atEnd()) {
        const QString line = input.readLine();
        const qsizetype separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            continue;
        }
        const QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();

        if (key == QStringLiteral("version")) {
            bool ok = false;
            const int parsed = value.toInt(&ok);
            if (!ok || parsed < 1) {
                setError(error, QStringLiteral("设置文件版本号无效。"));
                return false;
            }
            schemaVersion = parsed;
        } else if (key == QStringLiteral("api_url")) {
            loaded.apiUrl = value;
        } else if (key == QStringLiteral("api_key")) {
            loaded.legacyApiKey = value;
        } else if (key == QStringLiteral("model")) {
            loaded.model = value;
        } else if (key == QStringLiteral("ui_font_family")) {
            loaded.uiFontFamily = value;
        } else if (key == QStringLiteral("ui_font_size")) {
            loaded.uiFontSize = normalizedFontSize(value.toInt());
        } else if (key == QStringLiteral("allow_local_http")) {
            loaded.allowLocalHttp = value == QStringLiteral("1");
        } else if (key == QStringLiteral("reduce_motion")) {
            loaded.reduceMotion = value == QStringLiteral("1");
        }
    }

    if (schemaVersion > AppConfig::CurrentSchemaVersion) {
        setError(error, QStringLiteral("设置文件来自更高版本，当前程序不会覆盖它。"));
        return false;
    }
    if (!isSingleLineWithin(loaded.apiUrl, AppConfig::MaximumUrlLength) ||
        !isSingleLineWithin(loaded.model, AppConfig::MaximumModelLength) ||
        !isSingleLineWithin(loaded.uiFontFamily, AppConfig::MaximumFontFamilyLength) ||
        !isSingleLineWithin(loaded.legacyApiKey, AppConfig::MaximumLegacyKeyLength)) {
        setError(error, QStringLiteral("设置文件包含超长或非法字段。"));
        return false;
    }
    if (loaded.uiFontFamily.isEmpty()) {
        loaded.uiFontFamily = defaultConfig().uiFontFamily;
    }

    *config = std::move(loaded);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool saveConfig(const QString &path, const AppConfig &config, QString *error) {
    if (path.trimmed().isEmpty()) {
        setError(error, QStringLiteral("设置文件路径无效。"));
        return false;
    }
    if (!validateForSave(config, error)) {
        return false;
    }

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(error, QStringLiteral("设置文件无法写入：%1").arg(file.errorString()));
        return false;
    }

    QTextStream output(&file);
    output.setEncoding(QStringConverter::Utf8);
    output << "version=" << AppConfig::CurrentSchemaVersion << "\n";
    output << "api_url=" << config.apiUrl << "\n";
    output << "model=" << config.model << "\n";
    output << "ui_font_family=" << config.uiFontFamily << "\n";
    output << "ui_font_size=" << config.uiFontSize << "\n";
    output << "allow_local_http=" << (config.allowLocalHttp ? 1 : 0) << "\n";
    output << "reduce_motion=" << (config.reduceMotion ? 1 : 0) << "\n";
    output.flush();
    if (output.status() != QTextStream::Ok || !file.commit()) {
        setError(error, QStringLiteral("设置文件原子保存失败：%1").arg(file.errorString()));
        return false;
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}
