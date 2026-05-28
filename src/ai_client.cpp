#include "ai_client.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

static QString from_wide(const wchar_t *value) {
    return QString::fromWCharArray(value != nullptr ? value : L"");
}

static void copy_wide(const QString &value, wchar_t *dest, size_t dest_count) {
    if (dest == nullptr || dest_count == 0) {
        return;
    }
    const QString trimmed = value.left(static_cast<qsizetype>(dest_count - 1));
    const int copied = trimmed.toWCharArray(dest);
    dest[copied] = L'\0';
}

static void set_error(wchar_t *error, size_t error_count, const QString &message) {
    copy_wide(message, error, error_count);
}

static QByteArray build_request_body(const AppConfig *config, const wchar_t *requirement, const wchar_t *notes_text) {
    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"), QStringLiteral("你是便签总结助手，只基于用户提供的事件内容总结。")}
    });
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), QStringLiteral("用户要求：%1\n\n便签事件：\n%2")
            .arg(from_wide(requirement), from_wide(notes_text))}
    });

    const QJsonObject body{
        {QStringLiteral("model"), from_wide(config->model)},
        {QStringLiteral("messages"), messages},
        {QStringLiteral("temperature"), 0.3}
    };
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

static QString extract_error_message(const QByteArray &payload) {
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return QString::fromUtf8(payload).trimmed();
    }
    const QJsonObject root = doc.object();
    const QJsonObject error = root.value(QStringLiteral("error")).toObject();
    const QString message = error.value(QStringLiteral("message")).toString();
    return message.isEmpty() ? QString::fromUtf8(payload).trimmed() : message;
}

static QString extract_content_or_raw(const QByteArray &payload) {
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return QString::fromUtf8(payload).trimmed();
    }

    const QJsonObject root = doc.object();
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        const QJsonObject choice = choices.first().toObject();
        const QJsonObject message = choice.value(QStringLiteral("message")).toObject();
        const QString content = message.value(QStringLiteral("content")).toString();
        if (!content.isEmpty()) {
            return content;
        }
        const QString text = choice.value(QStringLiteral("text")).toString();
        if (!text.isEmpty()) {
            return text;
        }
    }

    const QString output_text = root.value(QStringLiteral("output_text")).toString();
    return output_text.isEmpty() ? QString::fromUtf8(payload).trimmed() : output_text;
}

int ai_client_summarize(
    const AppConfig *config,
    const wchar_t *requirement,
    const wchar_t *notes_text,
    wchar_t *result,
    size_t result_count,
    wchar_t *error,
    size_t error_count
) {
    if (result != nullptr && result_count > 0) {
        result[0] = L'\0';
    }
    if (!config_has_ai(config)) {
        set_error(error, error_count, QStringLiteral("AI 接口未配置，请先在设置中填写 API Key、URL 和模型名称。"));
        return 0;
    }

    const QUrl url(from_wide(config->api_url));
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        set_error(error, error_count, QStringLiteral("AI 接口 URL 无法解析。"));
        return 0;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "ChronoNotes/1.0");
    request.setRawHeader("Authorization", "Bearer " + from_wide(config->api_key).toUtf8());

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, build_request_body(config, requirement, notes_text));

    QEventLoop loop;
    QTimer timer;
    bool timed_out = false;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        timed_out = true;
        reply->abort();
        loop.quit();
    });
    timer.start(45000);
    loop.exec();
    timer.stop();

    const int status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError network_error = reply->error();
    const QString network_error_text = reply->errorString();
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (timed_out) {
        set_error(error, error_count, QStringLiteral("AI 请求超时，请检查网络或接口地址。"));
        return 0;
    }

    if (network_error != QNetworkReply::NoError && status_code == 0) {
        set_error(error, error_count, QStringLiteral("AI 请求失败：%1").arg(network_error_text));
        return 0;
    }

    if (status_code < 200 || status_code >= 300) {
        const QString message = extract_error_message(payload);
        set_error(error, error_count,
                  message.isEmpty()
                      ? QStringLiteral("AI 请求失败，HTTP 状态码：%1。").arg(status_code)
                      : QStringLiteral("AI 请求失败，HTTP 状态码：%1。%2").arg(status_code).arg(message));
        return 0;
    }

    const QString content = extract_content_or_raw(payload);
    if (content.isEmpty()) {
        set_error(error, error_count, QStringLiteral("AI 返回内容为空。"));
        return 0;
    }

    copy_wide(content, result, result_count);
    return 1;
}
