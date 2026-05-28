#ifndef QT_NOTE_APP_H
#define QT_NOTE_APP_H

#include <QAbstractListModel>
#include <QDate>
#include <QObject>
#include <QVector>
#include <QString>
#include <QUrl>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#include "config.h"
#include "note_view.h"
#include "note_store.h"

class NoteApp : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int stage READ stage WRITE setStage NOTIFY stageChanged)
    Q_PROPERTY(QString stageLabel READ stageLabel NOTIFY stageChanged)
    Q_PROPERTY(QString dateKey READ dateKey NOTIFY stageChanged)
    Q_PROPERTY(int completedCount READ completedCount NOTIFY countsChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(bool allCompleted READ allCompleted NOTIFY countsChanged)
    Q_PROPERTY(bool hasVisibleRows READ hasVisibleRows NOTIFY viewRevisionChanged)
    Q_PROPERTY(int viewRevision READ viewRevision NOTIFY viewRevisionChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchChanged)
    Q_PROPERTY(bool searchActive READ searchActive NOTIFY searchChanged)
    Q_PROPERTY(int searchCompletionFilter READ searchCompletionFilter WRITE setSearchCompletionFilter NOTIFY searchChanged)
    Q_PROPERTY(bool hasSelectedEvent READ hasSelectedEvent NOTIFY selectedEventChanged)
    Q_PROPERTY(int selectedEventId READ selectedEventId NOTIFY selectedEventChanged)
    Q_PROPERTY(QString selectedEventText READ selectedEventText NOTIFY selectedEventChanged)
    Q_PROPERTY(QString selectedEventMeta READ selectedEventMeta NOTIFY selectedEventChanged)
    Q_PROPERTY(QString selectedEventRepeat READ selectedEventRepeat NOTIFY selectedEventChanged)
    Q_PROPERTY(bool selectedEventReadOnly READ selectedEventReadOnly NOTIFY selectedEventChanged)
    Q_PROPERTY(QString apiUrl READ apiUrl WRITE setApiUrl NOTIFY configChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY configChanged)
    Q_PROPERTY(QString modelName READ modelName WRITE setModelName NOTIFY configChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TextRole,
        CompletedRole,
        MetaRole,
        SectionRole,
        SectionFirstRole,
        ReadOnlyRole,
        ArchiveRole,
        HighlightedTextRole,
        RepeatRole
    };

    explicit NoteApp(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int stage() const;
    void setStage(int value);
    QString stageLabel() const;
    QString dateKey() const;
    int completedCount() const;
    int totalCount() const;
    bool allCompleted() const;
    bool hasVisibleRows() const;
    int viewRevision() const;
    QString notice() const;
    QString searchQuery() const;
    void setSearchQuery(const QString &value);
    bool searchActive() const;
    int searchCompletionFilter() const;
    void setSearchCompletionFilter(int value);
    bool hasSelectedEvent() const;
    int selectedEventId() const;
    QString selectedEventText() const;
    QString selectedEventMeta() const;
    QString selectedEventRepeat() const;
    bool selectedEventReadOnly() const;
    bool canUndo() const;

    QString apiUrl() const;
    void setApiUrl(const QString &value);
    QString apiKey() const;
    void setApiKey(const QString &value);
    QString modelName() const;
    void setModelName(const QString &value);

    Q_INVOKABLE void addEvent(const QString &text);
    Q_INVOKABLE void toggleEvent(int id);
    Q_INVOKABLE void deleteEvent(int id);
    Q_INVOKABLE void updateEvent(int id, const QString &text);
    Q_INVOKABLE void toggleAll();
    Q_INVOKABLE void clearCompletedCurrent();
    Q_INVOKABLE void clearCompletedAll();
    Q_INVOKABLE void clearCurrentStage();
    Q_INVOKABLE void clearAllNotes();
    Q_INVOKABLE void undoLastAction();
    Q_INVOKABLE void selectEvent(int id, bool readOnly);
    Q_INVOKABLE void clearSelectedEvent();
    Q_INVOKABLE void saveSelectedEvent(const QString &text);
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE bool setEventRepeat(int id, const QString &repeat);
    Q_INVOKABLE bool exportJson();
    Q_INVOKABLE bool importJson();
    Q_INVOKABLE bool exportMarkdown();
    Q_INVOKABLE bool exportJsonToFile(const QUrl &fileUrl);
    Q_INVOKABLE bool importJsonFromFile(const QUrl &fileUrl);
    Q_INVOKABLE bool exportMarkdownToFile(const QUrl &fileUrl);
    Q_INVOKABLE int previewImportJsonEventCount(const QUrl &fileUrl);
    Q_INVOKABLE QString summarize(const QString &requirement);
    Q_INVOKABLE void summarizeAsync(const QString &requirement);

signals:
    void stageChanged();
    void countsChanged();
    void viewRevisionChanged();
    void noticeChanged();
    void searchChanged();
    void selectedEventChanged();
    void configChanged();
    void summaryReady(const QString &result);
    void undoChanged();

private:
    enum class UndoType {
        None,
        Toggle,
        Delete
    };

    using RowItem = NoteView::Row;

    NoteEvent *eventAt(int row) const;
    NoteEvent *findEvent(int id);
    const NoteEvent *findEventConst(int id) const;
    void syncSelectedEvent();
    void reload();
    void reloadModelChange();
    QVector<RowItem> buildRows();
    void saveNotes();
    void appendOperationLog(const QString &action, const NoteEvent *event = nullptr, const QString &detail = QString());
    void appendSummaryHistory(const QString &requirement, const QString &notes, const QString &result);
    void updateDateKey();
    void initPaths();
    void setNotice(const QString &value);
    void rememberUndo(UndoType type, const NoteEvent &event);
    void clearUndo();
    QString summaryNotes() const;
    QDate nextRepeatDate(const NoteEvent &event) const;
    void createNextRepeatIfNeeded(const NoteEvent &event);
    static QString summarizeWithConfig(const AppConfig &config, const QString &requirement, const QString &notes);
    static QString fromWide(const wchar_t *value);
    static std::wstring toWide(const QString &value);

    NoteStore store_{};
    AppConfig config_{};
    NoteStage stage_{NOTE_STAGE_DAY};
    wchar_t date_key_[NOTE_KEY_MAX]{};
    wchar_t data_dir_[MAX_PATH]{};
    wchar_t notes_path_[MAX_PATH]{};
    wchar_t sqlite_path_[MAX_PATH]{};
    wchar_t config_path_[MAX_PATH]{};
    wchar_t operation_log_path_[MAX_PATH]{};
    wchar_t summary_history_path_[MAX_PATH]{};
    QVector<RowItem> rows_;
    int view_revision_{0};
    QString notice_;
    QString search_query_;
    int search_completion_filter_{-1};
    int selected_event_id_{-1};
    bool selected_event_read_only_{false};
    UndoType undo_type_{UndoType::None};
    NoteEvent undo_event_{};
    qint64 undo_expires_at_{0};
};

#endif
