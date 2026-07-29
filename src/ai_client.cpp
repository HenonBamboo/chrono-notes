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

namespace {

constexpr qsizetype kMaximumRequestCharacters = 1024 * 1024;
constexpr qsizetype kMaximumResponseBytes = 1024 * 1024;
constexpr qsizetype kMaximumSummaryCharacters = 65536;

AiClientResult failure(const QString &message) {
    return {false, QString(), message};
}

bool isLoopbackHost(const QString &host) {
    const QString normalized = host.trimmed().toLower();
    return normalized == QStringLiteral("localhost") ||
           normalized == QStringLiteral("127.0.0.1") ||
           normalized == QStringLiteral("::1") ||
           normalized.endsWith(QStringLiteral(".localhost"));
}

QByteArray buildRequestBody(const AiClientRequest &request) {
    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"),
         QStringLiteral("你是便签和项目总结助手，只基于用户提供的上下文内容总结。")}
    });
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"),
         QStringLiteral("用户要求：%1\n\n上下文内容：\n%2")
             .arg(request.requirement, request.context)}
    });

    const QJsonObject body{
        {QStringLiteral("model"), request.model},
        {QStringLiteral("messages"), messages},
        {QStringLiteral("temperature"), 0.3}
    };
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QString extractErrorMessage(const QByteArray &payload) {
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return QString::fromUtf8(payload).trimmed();
    }
    const QJsonObject error =
        document.object().value(QStringLiteral("error")).toObject();
    const QString message = error.value(QStringLiteral("message")).toString();
    return message.isEmpty() ? QString::fromUtf8(payload).trimmed() : message;
}

QString extractContentOrRaw(const QByteArray &payload) {
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return QString::fromUtf8(payload).trimmed();
    }

    const QJsonObject root = document.object();
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        const QJsonObject choice = choices.first().toObject();
        const QJsonObject message =
            choice.value(QStringLiteral("message")).toObject();
        const QString content =
            message.value(QStringLiteral("content")).toString();
        if (!content.isEmpty()) {
            return content;
        }
        const QString text = choice.value(QStringLiteral("text")).toString();
        if (!text.isEmpty()) {
            return text;
        }
    }

    const QString outputText =
        root.value(QStringLiteral("output_text")).toString();
    return outputText.isEmpty()
               ? QString::fromUtf8(payload).trimmed()
               : outputText;
}

QString redactRemoteError(QString message, const AiClientRequest &request) {
    const QString redaction = QStringLiteral("[REDACTED]");
    if (!request.apiKey.isEmpty()) {
        message.replace(request.apiKey, redaction, Qt::CaseSensitive);
    }
    if (!request.requirement.isEmpty()) {
        message.replace(request.requirement, redaction, Qt::CaseSensitive);
    }
    if (!request.context.isEmpty()) {
        message.replace(request.context, redaction, Qt::CaseSensitive);
    }
    return message.left(512);
}

} // namespace

AiClientResult summarizeWithAi(const AiClientRequest &request,
                               const std::atomic_bool *cancelled) {
    if (cancelled != nullptr &&
        cancelled->load(std::memory_order_acquire)) {
        return failure(QStringLiteral("AI 请求已取消。"));
    }
    if (request.endpoint.trimmed().isEmpty() ||
        request.apiKey.trimmed().isEmpty() ||
        request.model.trimmed().isEmpty()) {
        return failure(QStringLiteral(
            "AI 接口未配置，请先在设置中填写 API Key、URL 和模型名称。"));
    }
    if (request.requirement.trimmed().isEmpty()) {
        return failure(QStringLiteral("总结要求不能为空。"));
    }
    if (request.context.trimmed().isEmpty()) {
        return failure(QStringLiteral("暂无可摘要内容。"));
    }
    if (request.requirement.size() + request.context.size() >
        kMaximumRequestCharacters) {
        return failure(QStringLiteral("AI 请求上下文超过 1 MiB 安全上限。"));
    }

    const QUrl url(request.endpoint);
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty() ||
        !url.userInfo().isEmpty()) {
        return failure(QStringLiteral("AI 接口 URL 无法解析或包含不安全的用户信息。"));
    }
    const bool secure =
        url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
    const bool explicitlyAllowedLocalHttp =
        request.allowLocalHttp &&
        url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 &&
        isLoopbackHost(url.host());
    if (!secure && !explicitlyAllowedLocalHttp) {
        return failure(QStringLiteral(
            "为保护 API Key，AI 地址必须使用 HTTPS；本机 localhost HTTP "
            "需在高级设置中显式允许。"));
    }

    const int timeoutMs = qBound(100, request.timeoutMs, 120000);
    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    networkRequest.setRawHeader("Accept", "application/json");
    networkRequest.setRawHeader("User-Agent", "ChronoNotes/1.0");
    networkRequest.setRawHeader(
        "Authorization", "Bearer " + request.apiKey.toUtf8());
    networkRequest.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::ManualRedirectPolicy);
    networkRequest.setTransferTimeout(timeoutMs);

    QNetworkAccessManager manager;
    QNetworkReply *reply =
        manager.post(networkRequest, buildRequestBody(request));

    QEventLoop loop;
    QTimer timeout;
    QTimer cancellationPoll;
    bool timedOut = false;
    bool wasCancelled = false;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop,
                     &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    cancellationPoll.setInterval(25);
    cancellationPoll.setTimerType(Qt::PreciseTimer);
    QObject::connect(&cancellationPoll, &QTimer::timeout, &loop, [&]() {
        if (cancelled == nullptr ||
            !cancelled->load(std::memory_order_acquire)) {
            return;
        }
        wasCancelled = true;
        reply->abort();
        loop.quit();
    });
    timeout.start(timeoutMs);
    if (cancelled != nullptr) {
        cancellationPoll.start();
    }
    loop.exec();
    timeout.stop();
    cancellationPoll.stop();

    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (wasCancelled) {
        return failure(QStringLiteral("AI 请求已取消。"));
    }
    if (timedOut) {
        return failure(
            QStringLiteral("AI 请求超时，请检查网络或接口地址。"));
    }
    if (networkError != QNetworkReply::NoError && statusCode == 0) {
        return failure(
            QStringLiteral("AI 请求失败：%1")
                .arg(redactRemoteError(networkErrorText, request)));
    }
    if (payload.size() > kMaximumResponseBytes) {
        return failure(QStringLiteral("AI 返回内容超过 1 MiB 安全上限。"));
    }
    if (statusCode < 200 || statusCode >= 300) {
        const QString message =
            redactRemoteError(extractErrorMessage(payload), request);
        return failure(
            message.isEmpty()
                ? QStringLiteral("AI 请求失败，HTTP 状态码：%1。")
                      .arg(statusCode)
                : QStringLiteral("AI 请求失败，HTTP 状态码：%1。%2")
                      .arg(statusCode)
                      .arg(message));
    }

    const QString content = extractContentOrRaw(payload);
    if (content.isEmpty()) {
        return failure(QStringLiteral("AI 返回内容为空。"));
    }
    if (content.size() > kMaximumSummaryCharacters) {
        return failure(
            QStringLiteral("AI 返回内容超过应用可保存的长度上限。"));
    }
    return {true, content, QString()};
}
