#ifndef QT_NOTE_APP_H
#define QT_NOTE_APP_H

#include <QAbstractListModel>
#include <QDate>
#include <QObject>
#include <QPair>
#include <QJsonObject>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QTimer>

#include <memory>
#include <atomic>

#include "config.h"
#include "clock.h"
#include "notebook.h"

class CredentialStore;
struct AiClientRequest;
namespace BackupService {
struct WorkspaceSnapshot;
}

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
    Q_PROPERTY(bool hasApiKey READ hasApiKey NOTIFY credentialsChanged)
    Q_PROPERTY(QString modelName READ modelName WRITE setModelName NOTIFY configChanged)
    Q_PROPERTY(bool allowLocalHttp READ allowLocalHttp WRITE setAllowLocalHttp NOTIFY configChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY configChanged)
    Q_PROPERTY(QString uiFontFamily READ uiFontFamily WRITE setUiFontFamily NOTIFY configChanged)
    Q_PROPERTY(int uiFontSize READ uiFontSize WRITE setUiFontSize NOTIFY configChanged)
    Q_PROPERTY(QStringList uiFontFamilies READ uiFontFamilies CONSTANT)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)
    Q_PROPERTY(QString dataDirectory READ dataDirectory CONSTANT)
    Q_PROPERTY(QString aiState READ aiState NOTIFY aiStateChanged)
    Q_PROPERTY(QString aiError READ aiError NOTIFY aiStateChanged)
    Q_PROPERTY(bool aiBusy READ aiBusy NOTIFY aiStateChanged)

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
    explicit NoteApp(std::shared_ptr<Clock> clock, QObject *parent = nullptr);
    ~NoteApp() override;

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
    bool hasApiKey() const;
    QString modelName() const;
    void setModelName(const QString &value);
    bool allowLocalHttp() const;
    void setAllowLocalHttp(bool value);
    bool reduceMotion() const;
    void setReduceMotion(bool value);
    QString uiFontFamily() const;
    void setUiFontFamily(const QString &value);
    int uiFontSize() const;
    void setUiFontSize(int value);
    QStringList uiFontFamilies() const;
    QString dataDirectory() const;
    QString aiState() const;
    QString aiError() const;
    bool aiBusy() const;

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
    Q_INVOKABLE bool saveConfig();
    Q_INVOKABLE bool saveSettings(const QString &newApiKey);
    Q_INVOKABLE bool replaceApiKey(const QString &value);
    Q_INVOKABLE bool clearApiKey();
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
    Q_INVOKABLE void summarizeContextAsync(const QString &requirement, const QString &context);
    Q_INVOKABLE void cancelSummary();
    Q_INVOKABLE void refreshTemporalState();

    bool readWorkspaceSnapshot(BackupService::WorkspaceSnapshot *snapshot,
                               QString *error) const;
    bool applyWorkspaceSnapshot(const BackupService::WorkspaceSnapshot &snapshot,
                                QString *error);
    QJsonObject diagnosticsSnapshot() const;

signals:
    void stageChanged();
    void countsChanged();
    void viewRevisionChanged();
    void noticeChanged();
    void searchChanged();
    void selectedEventChanged();
    void configChanged();
    void credentialsChanged();
    void summaryReady(const QString &result);
    void aiStateChanged();
    void operationFailed(const QString &code, const QString &message, bool recoverable);
    void undoChanged();

private:
    enum class UndoType {
        None,
        Toggle,
        Delete
    };

    struct RowItem {
        NotebookNote note;
        bool archive{};
        QString section;
        QString meta;
        bool section_first{};
        QString highlighted_text;
    };

    const NotebookNote *eventAt(int row) const;
    std::optional<NotebookNote> findEvent(int id) const;
    void syncSelectedEvent();
    void reload();
    void reloadModelChange();
    QVector<RowItem> buildRows();
    void reportMutationFailure(const QString &code, const NotebookMutation &mutation, const QString &fallbackMessage);
    void appendOperationLog(const QString &action, const NotebookNote *event = nullptr, const QString &detail = QString());
    void appendSummaryHistory(const QString &requirement, const QString &notes, const QString &result);
    void updateDateKey();
    void scheduleDateRefresh();
    void refreshDateIfNeeded();
    void initPaths();
    void setNotice(const QString &value);
    void rememberUndo(UndoType type, const NotebookNote &event, int derivedEventId = -1);
    void clearUndo();
    QString summaryNotes() const;
    void setAiState(const QString &state, const QString &error = QString());
    AiClientRequest aiRequest(const QString &requirement,
                              const QString &context) const;
    static QPair<bool, QString> summarizeRequest(
        const AiClientRequest &request,
        const std::shared_ptr<std::atomic_bool> &cancelToken = {});

    Notebook notebook_;
    AppConfig config_{};
    AppConfig persisted_config_{};
    bool config_read_only_{false};
    std::unique_ptr<CredentialStore> credential_store_;
    NotebookStage stage_{NotebookStage::Day};
    QString date_key_;
    QString data_dir_;
    QString notes_path_;
    QString sqlite_path_;
    QString config_path_;
    QString operation_log_path_;
    QString summary_history_path_;
    QString backup_dir_;
    QVector<RowItem> rows_;
    int view_revision_{0};
    QString notice_;
    QString search_query_;
    int search_completion_filter_{-1};
    int selected_event_id_{-1};
    bool selected_event_read_only_{false};
    UndoType undo_type_{UndoType::None};
    NotebookNote undo_event_{};
    int undo_derived_event_id_{-1};
    qint64 undo_expires_at_{0};
    QTimer date_refresh_timer_;
    std::shared_ptr<Clock> clock_;
    QString ai_state_{QStringLiteral("idle")};
    QString ai_error_;
    quint64 ai_request_id_{0};
    std::shared_ptr<std::atomic_bool> ai_cancel_token_;
};

#endif
