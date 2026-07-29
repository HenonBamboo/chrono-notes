#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include <QString>

#include <atomic>

struct AiClientRequest {
    QString endpoint;
    QString apiKey;
    QString model;
    QString requirement;
    QString context;
    bool allowLocalHttp{false};
    int timeoutMs{45000};
};

struct AiClientResult {
    bool ok{false};
    QString content;
    QString error;
};

AiClientResult summarizeWithAi(const AiClientRequest &request,
                               const std::atomic_bool *cancelled = nullptr);

#endif
