#ifndef BACKUP_SERVICE_H
#define BACKUP_SERVICE_H

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

#include "notebook.h"
#include "note_store.h"

#include <cstddef>

namespace BackupService {

struct WorkspaceNote {
    int id{};
    int stage{};
    QString dateKey;
    QString text;
    bool completed{};
    qint64 completedAt{};
    qint64 createdAt{};
    qint64 updatedAt{};
    QString repeat;
    QString seriesId;
};

struct WorkspaceSnapshot {
    QString createdAt;
    QString appVersion;
    QList<WorkspaceNote> notes;
    QJsonObject projects;
    QJsonArray summaries;
    QJsonObject preferences;
};

QString localPathFromUrl(const QUrl &url);
int previewJsonEventCount(const QUrl &fileUrl, QString *error);
bool exportJson(const NoteStore &store, const QUrl &fileUrl, QString *path, QString *error);
bool importJson(const QUrl &fileUrl, NoteStore *store, QString *path, QString *error);
bool exportMarkdown(const NoteStore &store, const QUrl &fileUrl, QString *path, QString *error);
bool exportJson(const NotebookSnapshot &snapshot, const QUrl &fileUrl,
                QString *path, QString *error);
bool importJson(const QUrl &fileUrl, NotebookSnapshot *snapshot,
                QString *path, QString *error);
bool importJson(const QUrl &fileUrl, std::nullptr_t,
                QString *path, QString *error);
bool exportMarkdown(const NotebookSnapshot &snapshot, const QUrl &fileUrl,
                    QString *path, QString *error);
QByteArray encodeWorkspaceV2(const WorkspaceSnapshot &snapshot, QString *error);
bool decodeWorkspaceV2(const QByteArray &bytes, WorkspaceSnapshot *snapshot, QString *error);
bool exportWorkspaceV2(const WorkspaceSnapshot &snapshot, const QUrl &fileUrl,
                       QString *path, QString *error);
bool importWorkspaceV2(const QUrl &fileUrl, WorkspaceSnapshot *snapshot,
                       QString *path, QString *error);

}

#endif
