#include "ai_client.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <atomic>
#include <memory>

namespace {

AiClientRequest localRequest(quint16 port) {
    AiClientRequest request;
    request.endpoint =
        QStringLiteral("http://127.0.0.1:%1/v1/chat/completions").arg(port);
    request.apiKey = QStringLiteral("test-secret");
    request.model = QStringLiteral("test-model");
    request.requirement = QStringLiteral("总结");
    request.context = QStringLiteral("测试上下文");
    request.allowLocalHttp = true;
    return request;
}

AiClientResult runRequest(AiClientRequest request,
                          const std::shared_ptr<std::atomic_bool> &cancelled,
                          int timeoutMs) {
    request.timeoutMs = timeoutMs;
    return summarizeWithAi(request, cancelled.get());
}

} // namespace

class AiClientTests : public QObject {
    Q_OBJECT

private slots:
    void rejectsNonHttpsRemoteEndpoints();
    void cancellationAbortsPendingRequest();
    void timeoutAbortsPendingRequest();
    void remoteErrorsDoNotExposeSensitiveInput();
    void rejectsCredentialsEmbeddedInUrl();
    void rejectsOversizedContext();
};

void AiClientTests::rejectsNonHttpsRemoteEndpoints() {
    AiClientRequest request;
    request.endpoint =
        QStringLiteral("http://example.com/v1/chat/completions");
    request.apiKey = QStringLiteral("secret-never-log");
    request.model = QStringLiteral("model");
    request.requirement = QStringLiteral("总结");
    request.context = QStringLiteral("内容");
    request.allowLocalHttp = true;

    auto cancelled = std::make_shared<std::atomic_bool>(false);
    const AiClientResult result = runRequest(request, cancelled, 500);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("HTTPS")));
    QVERIFY(!result.error.contains(QStringLiteral("secret-never-log")));
}

void AiClientTests::cancellationAbortsPendingRequest() {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        while (server.hasPendingConnections()) {
            QTcpSocket *socket = server.nextPendingConnection();
            socket->setParent(&server);
            connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                socket->readAll();
            });
        }
    });

    const AiClientRequest request = localRequest(server.serverPort());
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    QFutureWatcher<AiClientResult> watcher;
    QElapsedTimer elapsed;
    elapsed.start();
    watcher.setFuture(QtConcurrent::run([request, cancelled]() {
        return runRequest(request, cancelled, 5000);
    }));
    QTimer::singleShot(150, this, [cancelled]() {
        cancelled->store(true, std::memory_order_release);
    });

    QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 3000);
    const AiClientResult result = watcher.result();
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("取消")));
    QVERIFY(elapsed.elapsed() < 2500);
}

void AiClientTests::timeoutAbortsPendingRequest() {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        while (server.hasPendingConnections()) {
            QTcpSocket *socket = server.nextPendingConnection();
            socket->setParent(&server);
            connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                socket->readAll();
            });
        }
    });

    const AiClientRequest request = localRequest(server.serverPort());
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    QFutureWatcher<AiClientResult> watcher;
    watcher.setFuture(QtConcurrent::run([request, cancelled]() {
        return runRequest(request, cancelled, 150);
    }));

    QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 3000);
    const AiClientResult result = watcher.result();
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("超时")));
}

void AiClientTests::remoteErrorsDoNotExposeSensitiveInput() {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        while (server.hasPendingConnections()) {
            QTcpSocket *socket = server.nextPendingConnection();
            socket->setParent(&server);
            connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                socket->readAll();
                if (socket->property("responded").toBool()) {
                    return;
                }
                socket->setProperty("responded", true);
                const QByteArray payload = QByteArrayLiteral(
                    R"({"error":{"message":"credential test-secret and 测试上下文 were rejected"}})");
                const QByteArray response =
                    QByteArrayLiteral("HTTP/1.1 400 Bad Request\r\n"
                                      "Content-Type: application/json\r\n"
                                      "Connection: close\r\n"
                                      "Content-Length: ") +
                    QByteArray::number(payload.size()) +
                    QByteArrayLiteral("\r\n\r\n") + payload;
                socket->write(response);
                socket->disconnectFromHost();
            });
        }
    });

    const AiClientRequest request = localRequest(server.serverPort());
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    QFutureWatcher<AiClientResult> watcher;
    watcher.setFuture(QtConcurrent::run([request, cancelled]() {
        return runRequest(request, cancelled, 1000);
    }));

    QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 3000);
    const AiClientResult result = watcher.result();
    QVERIFY(!result.ok);
    QVERIFY(!result.error.contains(QStringLiteral("test-secret")));
    QVERIFY(!result.error.contains(QStringLiteral("测试上下文")));
    QVERIFY(result.error.contains(QStringLiteral("[REDACTED]")));
}

void AiClientTests::rejectsCredentialsEmbeddedInUrl() {
    AiClientRequest request;
    request.endpoint =
        QStringLiteral("https://user:password@example.com/v1/chat/completions");
    request.apiKey = QStringLiteral("test-secret");
    request.model = QStringLiteral("test-model");
    request.requirement = QStringLiteral("总结");
    request.context = QStringLiteral("内容");

    const AiClientResult result = summarizeWithAi(request);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("用户信息")));
    QVERIFY(!result.error.contains(QStringLiteral("password")));
}

void AiClientTests::rejectsOversizedContext() {
    AiClientRequest request;
    request.endpoint = QStringLiteral("https://example.com/v1/chat/completions");
    request.apiKey = QStringLiteral("test-secret");
    request.model = QStringLiteral("test-model");
    request.requirement = QStringLiteral("总结");
    request.context = QString(1024 * 1024, QLatin1Char('x'));

    const AiClientResult result = summarizeWithAi(request);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("1 MiB")));
}

QTEST_MAIN(AiClientTests)
#include "ai_client_tests.moc"
