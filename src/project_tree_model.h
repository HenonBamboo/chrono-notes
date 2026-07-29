#ifndef PROJECT_TREE_MODEL_H
#define PROJECT_TREE_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

class ProjectTreeModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int totalTasks READ totalTasks NOTIFY treeChanged)
    Q_PROPERTY(int completedTasks READ completedTasks NOTIFY treeChanged)
    Q_PROPERTY(int projectCount READ projectCount NOTIFY treeChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)
    Q_PROPERTY(int selectedNodeId READ selectedNodeId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedNodeTitle READ selectedNodeTitle NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedNodeKind READ selectedNodeKind NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedNodeDescription READ selectedNodeDescription NOTIFY selectionChanged)
    Q_PROPERTY(QStringList selectedAncestorPath READ selectedAncestorPath NOTIFY selectionChanged)
    Q_PROPERTY(int selectedNodeChildCount READ selectedNodeChildCount NOTIFY treeChanged)
    Q_PROPERTY(bool selectedNodeCompleted READ selectedNodeCompleted NOTIFY treeChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ParentIdRole,
        TitleRole,
        KindRole,
        CompletedRole,
        DepthRole,
        ExpandedRole,
        ChildCountRole,
        TotalTasksRole,
        CompletedTasksRole,
        DescriptionRole
    };

    explicit ProjectTreeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int totalTasks() const;
    int completedTasks() const;
    int projectCount() const;
    QString lastError() const;
    Q_INVOKABLE bool reloadFromDisk();
    QJsonObject exportSnapshot() const;
    bool importSnapshot(const QJsonObject &snapshot);
    int selectedNodeId() const;
    QString selectedNodeTitle() const;
    QString selectedNodeKind() const;
    QString selectedNodeDescription() const;
    QStringList selectedAncestorPath() const;
    int selectedNodeChildCount() const;
    bool selectedNodeCompleted() const;
    Q_INVOKABLE QStringList nodePath(int id) const;
    Q_INVOKABLE bool selectNode(int id);

    Q_INVOKABLE int addProject(const QString &title);
    Q_INVOKABLE int addChild(int parentId, const QString &title);
    Q_INVOKABLE bool updateTitle(int id, const QString &title);
    Q_INVOKABLE bool updateDescription(int id, const QString &description);
    Q_INVOKABLE bool toggleComplete(int id);
    Q_INVOKABLE bool toggleExpanded(int id);
    Q_INVOKABLE bool removeNode(int id);

signals:
    void treeChanged();
    void errorChanged();
    void persistenceError(const QString &message);
    void selectionChanged();

private:
    struct Node {
        int id{};
        int parent_id{};
        QString title;
        QString kind;
        QString description;
        bool completed{};
        bool expanded{true};
        qint64 created_at{};
        qint64 updated_at{};
        int child_count{};
        int total_tasks{};
        int completed_tasks{};
    };

    struct Row {
        int node_index{};
        int depth{};
    };

    int findNodeIndex(int id) const;
    bool hasChildren(int id) const;
    int directChildCount(int id) const;
    int descendantTaskCount(int id) const;
    int completedDescendantTaskCount(int id) const;
    void appendVisibleRows(int parentId, int depth, QVector<Row> *rows) const;
    void rebuildCaches();
    void rebuildRows();
    void resetToRows(QVector<Row> rows);
    bool save();
    bool load();
    bool parseSnapshot(const QJsonObject &snapshot, QVector<Node> *nodes,
                       int *nextId, QString *error) const;
    void setLastError(const QString &message, bool emitFailure = true);
    QString storagePath() const;
    int nextId() const;
    void removeDescendants(int id);
    bool isDescendantOf(int id, int parentId) const;

    QVector<Node> nodes_;
    QVector<Row> rows_;
    QHash<int, int> node_index_by_id_;
    QHash<int, QVector<int>> children_by_parent_;
    int next_id_{1};
    QString storage_path_;
    QString last_error_;
    int selected_node_id_{-1};
    int total_tasks_{};
    int completed_tasks_{};
    int project_count_{};
};

#endif
