#ifndef NOTE_VIEW_H
#define NOTE_VIEW_H

#include <QDate>
#include <QString>
#include <QVector>

#include "note_store.h"

namespace NoteView {

struct Row {
    NoteEvent *event{};
    bool archive{};
    QString section;
    QString meta;
    bool section_first{};
    QString highlighted_text;
};

struct BuildRequest {
    NoteStage stage{NOTE_STAGE_DAY};
    QString date_key;
    QString search_query;
    int search_completion_filter{-1};
    QDate today{QDate::currentDate()};
};

QVector<Row> buildRows(NoteStore *store, const BuildRequest &request);
bool eventBelongsToArchive(const NoteEvent *event, NoteStage stage, const QString &dateKey);
QString metaFor(const NoteEvent *event, const QDate &today = QDate::currentDate());
QString archiveMetaFor(const NoteEvent *event);
QString searchMetaFor(const NoteEvent *event);
QString highlightedTextFor(const NoteEvent *event, const QString &searchQuery);
QString sectionFor(const NoteEvent *event, bool archive);
QString fromWide(const wchar_t *value);

}

#endif
