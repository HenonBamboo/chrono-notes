#ifndef PROJECT_TREE_MODEL_H
#define PROJECT_TREE_MODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class ProjectTreeModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int totalTasks READ totalTasks NOTIFY treeChanged)
    Q_PROPERTY(int completedTasks READ completedTasks NOTIFY treeChanged)
    Q_PROPERTY(int projectCount READ projectCount NOTIFY treeChanged)

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
        CompletedTasksRole
    };

    explicit ProjectTreeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int totalTasks() const;
    int completedTasks() const;
    int projectCount() const;

    Q_INVOKABLE int addProject(const QString &title);
    Q_INVOKABLE int addChild(int parentId, const QString &title);
    Q_INVOKABLE bool updateTitle(int id, const QString &title);
    Q_INVOKABLE bool toggleComplete(int id);
    Q_INVOKABLE bool toggleExpanded(int id);
    Q_INVOKABLE bool removeNode(int id);

signals:
    void treeChanged();

private:
    struct Node {
        int id{};
        int parent_id{};
        QString title;
        QString kind;
        bool completed{};
        bool expanded{true};
        qint64 created_at{};
        qint64 updated_at{};
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
    void rebuildRows();
    void resetToRows(QVector<Row> rows);
    void save() const;
    void load();
    QString storagePath() const;
    int nextId() const;
    void removeDescendants(int id);
    bool isDescendantOf(int id, int parentId) const;

    QVector<Node> nodes_;
    QVector<Row> rows_;
    int next_id_{1};
    QString storage_path_;
};

#endif
