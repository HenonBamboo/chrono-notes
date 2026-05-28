#ifndef BACKUP_SERVICE_H
#define BACKUP_SERVICE_H

#include <QString>
#include <QUrl>

#include "note_store.h"

namespace BackupService {

QString localPathFromUrl(const QUrl &url);
int previewJsonEventCount(const QUrl &fileUrl, QString *error);
bool exportJson(const NoteStore &store, const QUrl &fileUrl, QString *path, QString *error);
bool importJson(const QUrl &fileUrl, NoteStore *store, QString *path, QString *error);
bool exportMarkdown(const NoteStore &store, const QUrl &fileUrl, QString *path, QString *error);

}

#endif
