#include "backup_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {

QString fromWide(const wchar_t *value) {
    return QString::fromWCharArray(value);
}

std::wstring toWide(const QString &value) {
    return value.toStdWString();
}

bool ensureParentDirectory(const QString &path) {
    const QFileInfo info(path);
    const QDir dir = info.absoluteDir();
    return dir.exists() || QDir().mkpath(dir.absolutePath());
}

qint64 jsonInt64(const QJsonObject &object, const QString &key) {
    const QJsonValue value = object.value(key);
    if (value.isString()) {
        return value.toString().toLongLong();
    }
    return static_cast<qint64>(value.toDouble());
}

bool readJsonEvents(const QUrl &fileUrl, QJsonArray *events, QString *path, QString *error) {
    const QString resolvedPath = BackupService::localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("请选择要导入的 JSON 文件。");
        }
        return false;
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("没有找到 JSON 文件。");
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject() || !doc.object().value(QStringLiteral("events")).isArray()) {
        if (error != nullptr) {
            *error = QStringLiteral("JSON 格式无效。");
        }
        return false;
    }

    if (events != nullptr) {
        *events = doc.object().value(QStringLiteral("events")).toArray();
    }
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

}

namespace BackupService {

QString localPathFromUrl(const QUrl &url) {
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return url.toString(QUrl::PreferLocalFile);
}

int previewJsonEventCount(const QUrl &fileUrl, QString *error) {
    QJsonArray events;
    if (!readJsonEvents(fileUrl, &events, nullptr, error)) {
        return -1;
    }
    return events.size();
}

bool exportJson(const NoteStore &store, const QUrl &fileUrl, QString *path, QString *error) {
    const QString resolvedPath = localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("请选择 JSON 导出位置。");
        }
        return false;
    }

    QJsonArray events;
    for (int i = 0; i < store.count; ++i) {
        const NoteEvent &event = store.items[i];
        QJsonObject item;
        item.insert(QStringLiteral("id"), event.id);
        item.insert(QStringLiteral("stage"), event.stage);
        item.insert(QStringLiteral("dateKey"), fromWide(event.date_key));
        item.insert(QStringLiteral("text"), fromWide(event.text));
        item.insert(QStringLiteral("completed"), event.completed != 0);
        item.insert(QStringLiteral("completedAt"), QString::number(event.completed_at));
        item.insert(QStringLiteral("createdAt"), QString::number(event.created_at));
        item.insert(QStringLiteral("updatedAt"), QString::number(event.updated_at));
        item.insert(QStringLiteral("repeat"), fromWide(event.repeat));
        events.append(item);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("events"), events);

    if (!ensureParentDirectory(resolvedPath)) {
        if (error != nullptr) {
            *error = QStringLiteral("JSON 导出目录不可用。");
        }
        return false;
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = QStringLiteral("JSON 导出失败。");
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

bool importJson(const QUrl &fileUrl, NoteStore *store, QString *path, QString *error) {
    if (store == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("导入目标不可用。");
        }
        return false;
    }

    QJsonArray events;
    QString resolvedPath;
    if (!readJsonEvents(fileUrl, &events, &resolvedPath, error)) {
        return false;
    }

    NoteStore imported{};
    note_store_init(&imported);
    for (const QJsonValue &value : events) {
        const QJsonObject item = value.toObject();
        NoteEvent event{};
        event.id = item.value(QStringLiteral("id")).toInt();
        event.stage = static_cast<NoteStage>(item.value(QStringLiteral("stage")).toInt());
        const std::wstring date = toWide(item.value(QStringLiteral("dateKey")).toString());
        const std::wstring text = toWide(item.value(QStringLiteral("text")).toString());
        const std::wstring repeat = toWide(item.value(QStringLiteral("repeat")).toString());
        wcsncpy(event.date_key, date.c_str(), NOTE_KEY_MAX - 1);
        wcsncpy(event.text, text.c_str(), NOTE_TEXT_MAX - 1);
        wcsncpy(event.repeat, repeat.c_str(), NOTE_REPEAT_MAX - 1);
        event.date_key[NOTE_KEY_MAX - 1] = L'\0';
        event.text[NOTE_TEXT_MAX - 1] = L'\0';
        event.repeat[NOTE_REPEAT_MAX - 1] = L'\0';
        event.completed = item.value(QStringLiteral("completed")).toBool() ? 1 : 0;
        event.completed_at = jsonInt64(item, QStringLiteral("completedAt"));
        event.created_at = jsonInt64(item, QStringLiteral("createdAt"));
        event.updated_at = jsonInt64(item, QStringLiteral("updatedAt"));
        note_store_restore(&imported, &event);
    }

    *store = imported;
    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

bool exportMarkdown(const NoteStore &store, const QUrl &fileUrl, QString *path, QString *error) {
    const QString resolvedPath = localPathFromUrl(fileUrl);
    if (resolvedPath.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("请选择 Markdown 导出位置。");
        }
        return false;
    }
    if (!ensureParentDirectory(resolvedPath)) {
        if (error != nullptr) {
            *error = QStringLiteral("Markdown 导出目录不可用。");
        }
        return false;
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = QStringLiteral("Markdown 导出失败。");
        }
        return false;
    }

    QTextStream out(&file);
    out << "# ChronoNotes 导出\n\n";
    for (int stage = NOTE_STAGE_DAY; stage <= NOTE_STAGE_YEAR; ++stage) {
        out << "## " << fromWide(note_stage_label(static_cast<NoteStage>(stage))) << "\n\n";
        for (int i = 0; i < store.count; ++i) {
            const NoteEvent &event = store.items[i];
            if (event.stage != stage) {
                continue;
            }
            out << "- [" << (event.completed ? "x" : " ") << "] "
                << fromWide(event.date_key) << " "
                << fromWide(event.text);
            if (event.repeat[0] != L'\0') {
                out << " (repeat: " << fromWide(event.repeat) << ")";
            }
            out << "\n";
        }
        out << "\n";
    }

    if (path != nullptr) {
        *path = resolvedPath;
    }
    return true;
}

}
