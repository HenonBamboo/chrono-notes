#ifndef NOTEBOOK_H
#define NOTEBOOK_H

#include <QList>
#include <QString>
#include <QtGlobal>

#include <memory>
#include <optional>

enum class NotebookStage {
    Day = 0,
    Week = 1,
    Month = 2,
    Year = 3
};

enum class NotebookRepeat {
    None,
    Daily,
    Weekly,
    Monthly,
    Yearly
};

enum class NotebookCompletion {
    Any,
    Open,
    Completed
};

enum class NotebookBulkAction {
    ToggleCompletion,
    Complete,
    Uncomplete,
    Remove
};

enum class NotebookErrorCode {
    None,
    NotOpen,
    InvalidArgument,
    NotFound,
    Conflict,
    Persistence,
    CorruptData,
    UnsupportedSchema
};

struct NotebookError {
    NotebookErrorCode code{NotebookErrorCode::None};
    QString message;

    bool isError() const noexcept {
        return code != NotebookErrorCode::None;
    }
};

template <typename T>
struct NotebookOutcome {
    T value{};
    NotebookError error{};

    bool ok() const noexcept {
        return !error.isError();
    }
};

struct NotebookNote {
    int id{};
    NotebookStage stage{NotebookStage::Day};
    QString dateKey;
    QString text;
    bool completed{};
    qint64 completedAt{};
    qint64 createdAt{};
    qint64 updatedAt{};
    NotebookRepeat repeat{NotebookRepeat::None};
    QString seriesId;

    bool operator==(const NotebookNote &other) const {
        return id == other.id &&
               stage == other.stage &&
               dateKey == other.dateKey &&
               text == other.text &&
               completed == other.completed &&
               completedAt == other.completedAt &&
               createdAt == other.createdAt &&
               updatedAt == other.updatedAt &&
               repeat == other.repeat &&
               seriesId == other.seriesId;
    }
};

struct NotebookDraft {
    NotebookStage stage{NotebookStage::Day};
    QString dateKey;
    QString text;
    NotebookRepeat repeat{NotebookRepeat::None};
    QString seriesId;
};

struct NotebookUpdate {
    std::optional<QString> text;
    std::optional<NotebookRepeat> repeat;
};

struct NotebookQuery {
    std::optional<NotebookStage> stage;
    QString dateKey;
    QString searchText;
    NotebookCompletion completion{NotebookCompletion::Any};
    int limit{};
};

struct NotebookSnapshot {
    int schemaVersion{2};
    QList<NotebookNote> notes;
};

struct NotebookMutation {
    NotebookError error{};
    int affected{};
    int noteId{-1};
    QList<int> derivedNoteIds;

    bool ok() const noexcept {
        return !error.isError();
    }
};

// Deep Module for note lifecycle and persistence. Its Interface is value-only;
// SQLite, the legacy NoteStore and all runtime containers remain Implementation.
// A Notebook instance must be used from the thread on which it is opened.
class Notebook final {
public:
    static constexpr qsizetype MaxTextLength = 65536;
    static constexpr int CurrentSchemaVersion = 2;

    Notebook();
    ~Notebook();
    Notebook(Notebook &&other) noexcept;
    Notebook &operator=(Notebook &&other) noexcept;

    Notebook(const Notebook &) = delete;
    Notebook &operator=(const Notebook &) = delete;

    NotebookError open(const QString &databasePath);
    void close();
    bool isOpen() const noexcept;
    QString databasePath() const;
    int count() const noexcept;

    // Results are ordered open-first, then newest activity and newest id.
    NotebookOutcome<QList<NotebookNote>> query(const NotebookQuery &request = {}) const;
    NotebookOutcome<NotebookNote> note(int id) const;

    NotebookMutation add(const NotebookDraft &draft, qint64 timestampSeconds = 0);
    NotebookMutation update(int id,
                            const NotebookUpdate &update,
                            qint64 timestampSeconds = 0);
    // Completing a recurring daily note creates its next occurrence in the
    // same transaction and reports the new id in derivedNoteIds.
    NotebookMutation toggle(int id, qint64 timestampSeconds = 0);
    NotebookMutation remove(int id);
    NotebookMutation bulk(NotebookBulkAction action,
                          const NotebookQuery &request,
                          qint64 timestampSeconds = 0);
    NotebookMutation restore(const NotebookNote &note);
    // Replaces the snapshot and removes derived notes atomically. This is the
    // inverse operation for a recurring completion that created occurrences.
    NotebookMutation restoreReplacing(const NotebookNote &snapshot,
                                      const QList<int> &removeIds = {});

    NotebookOutcome<NotebookSnapshot> snapshot() const;
    // Schema 1 snapshots are accepted for legacy import and receive generated
    // series ids. Schema 2 snapshots require complete, unique identities.
    NotebookMutation importSnapshot(const NotebookSnapshot &snapshot);
    // One-time Adapter for STICKY_NOTES_C_V1/V2/V3 files. Legacy structs and
    // fixed buffers never cross this Interface.
    NotebookMutation importLegacyFile(const QString &path);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
