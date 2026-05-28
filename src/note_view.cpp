#include "note_view.h"

#include <QDateTime>

#include <algorithm>
#include <cwchar>

namespace NoteView {

QString fromWide(const wchar_t *value) {
    return value == nullptr ? QString() : QString::fromWCharArray(value);
}

bool eventBelongsToArchive(const NoteEvent *event, NoteStage stage, const QString &dateKey) {
    if (event == nullptr || event->stage != NOTE_STAGE_DAY || stage == NOTE_STAGE_DAY) {
        return false;
    }

    const QDate event_date = QDate::fromString(fromWide(event->date_key), QStringLiteral("yyyy-MM-dd"));
    if (!event_date.isValid()) {
        return false;
    }

    if (stage == NOTE_STAGE_WEEK) {
        int event_year = 0;
        const int event_week = event_date.weekNumber(&event_year);
        return QStringLiteral("%1-W%2")
            .arg(event_year, 4, 10, QLatin1Char('0'))
            .arg(event_week, 2, 10, QLatin1Char('0')) == dateKey;
    }
    if (stage == NOTE_STAGE_MONTH) {
        return event_date.toString(QStringLiteral("yyyy-MM")) == dateKey;
    }
    if (stage == NOTE_STAGE_YEAR) {
        return event_date.toString(QStringLiteral("yyyy")) == dateKey;
    }
    return false;
}

QString metaFor(const NoteEvent *event, const QDate &today) {
    if (event == nullptr) {
        return {};
    }
    if (event->completed && event->completed_at > 0) {
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(event->completed_at);
        return QStringLiteral("完成 %1").arg(dt.toString(QStringLiteral("MM-dd HH:mm")));
    }
    if (event->created_at > 0) {
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(event->created_at);
        if (dt.date() == today) {
            return QStringLiteral("今天");
        }
        return dt.toString(QStringLiteral("MM-dd"));
    }
    return QStringLiteral("刚刚创建");
}

QString archiveMetaFor(const NoteEvent *event) {
    if (event == nullptr) {
        return {};
    }
    const QString source_date = fromWide(event->date_key);
    if (event->completed && event->completed_at > 0) {
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(event->completed_at);
        return QStringLiteral("来自 %1 · 完成 %2").arg(source_date, dt.toString(QStringLiteral("MM-dd HH:mm")));
    }
    return QStringLiteral("来自 %1").arg(source_date);
}

QString searchMetaFor(const NoteEvent *event) {
    if (event == nullptr) {
        return {};
    }
    const QString stage = fromWide(note_stage_label(static_cast<NoteStage>(event->stage)));
    const QString source_date = fromWide(event->date_key);
    const QString state = event->completed ? QStringLiteral("已完成") : QStringLiteral("未完成");
    return QStringLiteral("%1 · %2 · %3").arg(stage, source_date, state);
}

QString highlightedTextFor(const NoteEvent *event, const QString &searchQuery) {
    const QString text = event != nullptr ? fromWide(event->text).toHtmlEscaped() : QString();
    const QString query = searchQuery.trimmed().toHtmlEscaped();
    if (query.isEmpty()) {
        return text;
    }

    QString highlighted;
    qsizetype cursor = 0;
    while (cursor < text.size()) {
        const qsizetype index = text.indexOf(query, cursor, Qt::CaseInsensitive);
        if (index < 0) {
            highlighted += text.mid(cursor);
            break;
        }
        highlighted += text.mid(cursor, index - cursor);
        highlighted += QStringLiteral("<mark>") + text.mid(index, query.size()) + QStringLiteral("</mark>");
        cursor = index + query.size();
    }
    return highlighted;
}

QString sectionFor(const NoteEvent *event, bool archive) {
    if (archive) {
        return QStringLiteral("自动收纳");
    }
    return event != nullptr && event->completed ? QStringLiteral("完成批注") : QStringLiteral("待处理");
}

QVector<Row> buildRows(NoteStore *store, const BuildRequest &request) {
    QVector<Row> rows;
    if (store == nullptr) {
        return rows;
    }

    const QString search = request.search_query.trimmed();
    if (!search.isEmpty()) {
        for (int i = 0; i < store->count; ++i) {
            NoteEvent *event = &store->items[i];
            const bool completion_matches = request.search_completion_filter < 0 ||
                                            (request.search_completion_filter == 1 && event->completed) ||
                                            (request.search_completion_filter == 0 && !event->completed);
            if (completion_matches && fromWide(event->text).contains(search, Qt::CaseInsensitive)) {
                rows.append(Row{event,
                                false,
                                QStringLiteral("搜索结果"),
                                searchMetaFor(event),
                                false,
                                highlightedTextFor(event, search)});
            }
        }
        std::sort(rows.begin(), rows.end(), [](const Row &left, const Row &right) {
            if (left.event == nullptr || right.event == nullptr) {
                return false;
            }
            const long long left_time = left.event->updated_at > 0 ? left.event->updated_at : left.event->created_at;
            const long long right_time = right.event->updated_at > 0 ? right.event->updated_at : right.event->created_at;
            if (left_time != right_time) {
                return left_time > right_time;
            }
            return left.event->id > right.event->id;
        });
        return rows;
    }

    QVector<NoteEvent *> filtered(std::max(1, store->count));
    const std::wstring date_key = request.date_key.toStdWString();
    const int count = note_store_filter(store, request.stage, date_key.c_str(), filtered.data(), filtered.size());
    for (int i = 0; i < count; ++i) {
        rows.append(Row{filtered[i],
                        false,
                        sectionFor(filtered[i], false),
                        metaFor(filtered[i], request.today),
                        false,
                        highlightedTextFor(filtered[i], QString())});
    }

    if (request.stage != NOTE_STAGE_DAY) {
        const qsizetype archive_start = rows.size();
        for (int i = 0; i < store->count; ++i) {
            NoteEvent *event = &store->items[i];
            if (eventBelongsToArchive(event, request.stage, request.date_key)) {
                rows.append(Row{event,
                                true,
                                sectionFor(event, true),
                                archiveMetaFor(event),
                                false,
                                highlightedTextFor(event, QString())});
            }
        }
        std::sort(rows.begin() + archive_start, rows.end(), [](const Row &left, const Row &right) {
            if (left.event == nullptr || right.event == nullptr) {
                return false;
            }
            const int date_cmp = std::wcscmp(left.event->date_key, right.event->date_key);
            if (date_cmp != 0) {
                return date_cmp > 0;
            }
            if (left.event->completed != right.event->completed) {
                return left.event->completed < right.event->completed;
            }
            const long long left_time = left.event->completed && left.event->completed_at > 0 ? left.event->completed_at : left.event->created_at;
            const long long right_time = right.event->completed && right.event->completed_at > 0 ? right.event->completed_at : right.event->created_at;
            return left_time > right_time || (left_time == right_time && left.event->id > right.event->id);
        });
    }

    QString previous_section;
    for (Row &row : rows) {
        row.section_first = row.section != previous_section;
        previous_section = row.section;
    }
    return rows;
}

}
