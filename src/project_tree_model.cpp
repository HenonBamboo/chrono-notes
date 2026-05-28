#include "project_tree_model.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

ProjectTreeModel::ProjectTreeModel(QObject *parent) : QAbstractListModel(parent) {
    storage_path_ = storagePath();
    load();
    rebuildRows();
}

int ProjectTreeModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : rows_.size();
}

QVariant ProjectTreeModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }
    const Row &row = rows_.at(index.row());
    if (row.node_index < 0 || row.node_index >= nodes_.size()) {
        return {};
    }
    const Node &node = nodes_.at(row.node_index);
    switch (role) {
        case IdRole: return node.id;
        case ParentIdRole: return node.parent_id;
        case TitleRole: return node.title;
        case KindRole: return node.kind;
        case CompletedRole: return node.completed;
        case DepthRole: return row.depth;
        case ExpandedRole: return node.expanded;
        case ChildCountRole: return directChildCount(node.id);
        case TotalTasksRole: return descendantTaskCount(node.id);
        case CompletedTasksRole: return completedDescendantTaskCount(node.id);
        default: return {};
    }
}

QHash<int, QByteArray> ProjectTreeModel::roleNames() const {
    return {
        {IdRole, "nodeId"},
        {ParentIdRole, "parentId"},
        {TitleRole, "title"},
        {KindRole, "kind"},
        {CompletedRole, "completed"},
        {DepthRole, "depth"},
        {ExpandedRole, "expanded"},
        {ChildCountRole, "childCount"},
        {TotalTasksRole, "totalTasks"},
        {CompletedTasksRole, "completedTasks"}
    };
}

int ProjectTreeModel::totalTasks() const {
    int total = 0;
    for (const Node &node : nodes_) {
        if (node.kind == QStringLiteral("task")) {
            ++total;
        }
    }
    return total;
}

int ProjectTreeModel::completedTasks() const {
    int total = 0;
    for (const Node &node : nodes_) {
        if (node.kind == QStringLiteral("task") && node.completed) {
            ++total;
        }
    }
    return total;
}

int ProjectTreeModel::projectCount() const {
    int total = 0;
    for (const Node &node : nodes_) {
        if (node.parent_id == 0) {
            ++total;
        }
    }
    return total;
}

int ProjectTreeModel::addProject(const QString &title) {
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const int id = next_id_++;
    nodes_.append(Node{id, 0, trimmed, QStringLiteral("project"), false, true, now, now});
    resetToRows({});
    save();
    return id;
}

int ProjectTreeModel::addChild(int parentId, const QString &title) {
    const int parent_index = findNodeIndex(parentId);
    const QString trimmed = title.trimmed();
    if (parent_index < 0 || trimmed.isEmpty()) {
        return -1;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const int id = next_id_++;
    nodes_[parent_index].expanded = true;
    nodes_[parent_index].updated_at = now;
    nodes_.append(Node{id, parentId, trimmed, QStringLiteral("task"), false, true, now, now});
    resetToRows({});
    save();
    return id;
}

bool ProjectTreeModel::updateTitle(int id, const QString &title) {
    const int index = findNodeIndex(id);
    const QString trimmed = title.trimmed();
    if (index < 0 || trimmed.isEmpty() || nodes_[index].title == trimmed) {
        return false;
    }
    nodes_[index].title = trimmed;
    nodes_[index].updated_at = QDateTime::currentSecsSinceEpoch();
    resetToRows({});
    save();
    return true;
}

bool ProjectTreeModel::toggleComplete(int id) {
    const int index = findNodeIndex(id);
    if (index < 0 || nodes_[index].kind == QStringLiteral("project")) {
        return false;
    }
    nodes_[index].completed = !nodes_[index].completed;
    nodes_[index].updated_at = QDateTime::currentSecsSinceEpoch();
    resetToRows({});
    save();
    return true;
}

bool ProjectTreeModel::toggleExpanded(int id) {
    const int index = findNodeIndex(id);
    if (index < 0 || !hasChildren(id)) {
        return false;
    }
    nodes_[index].expanded = !nodes_[index].expanded;
    resetToRows({});
    save();
    return true;
}

bool ProjectTreeModel::removeNode(int id) {
    const int index = findNodeIndex(id);
    if (index < 0) {
        return false;
    }
    removeDescendants(id);
    nodes_.removeAt(findNodeIndex(id));
    resetToRows({});
    save();
    return true;
}

int ProjectTreeModel::findNodeIndex(int id) const {
    for (int i = 0; i < nodes_.size(); ++i) {
        if (nodes_.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

bool ProjectTreeModel::hasChildren(int id) const {
    return directChildCount(id) > 0;
}

int ProjectTreeModel::directChildCount(int id) const {
    int total = 0;
    for (const Node &node : nodes_) {
        if (node.parent_id == id) {
            ++total;
        }
    }
    return total;
}

int ProjectTreeModel::descendantTaskCount(int id) const {
    const int index = findNodeIndex(id);
    if (index < 0) {
        return 0;
    }
    int total = nodes_.at(index).kind == QStringLiteral("task") ? 1 : 0;
    for (const Node &node : nodes_) {
        if (node.parent_id == id) {
            total += descendantTaskCount(node.id);
        }
    }
    return total;
}

int ProjectTreeModel::completedDescendantTaskCount(int id) const {
    const int index = findNodeIndex(id);
    if (index < 0) {
        return 0;
    }
    int total = nodes_.at(index).kind == QStringLiteral("task") && nodes_.at(index).completed ? 1 : 0;
    for (const Node &node : nodes_) {
        if (node.parent_id == id) {
            total += completedDescendantTaskCount(node.id);
        }
    }
    return total;
}

void ProjectTreeModel::appendVisibleRows(int parentId, int depth, QVector<Row> *rows) const {
    if (rows == nullptr) {
        return;
    }
    for (int i = 0; i < nodes_.size(); ++i) {
        const Node &node = nodes_.at(i);
        if (node.parent_id != parentId) {
            continue;
        }
        rows->append(Row{i, depth});
        if (node.expanded) {
            appendVisibleRows(node.id, depth + 1, rows);
        }
    }
}

void ProjectTreeModel::rebuildRows() {
    rows_.clear();
    appendVisibleRows(0, 0, &rows_);
}

void ProjectTreeModel::resetToRows(QVector<Row> rows) {
    beginResetModel();
    if (rows.isEmpty()) {
        rebuildRows();
    } else {
        rows_ = rows;
    }
    endResetModel();
    emit treeChanged();
}

void ProjectTreeModel::save() const {
    QDir().mkpath(QFileInfo(storage_path_).absolutePath());
    QFile file(storage_path_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    QJsonArray nodes;
    for (const Node &node : nodes_) {
        QJsonObject item;
        item.insert(QStringLiteral("id"), node.id);
        item.insert(QStringLiteral("parentId"), node.parent_id);
        item.insert(QStringLiteral("title"), node.title);
        item.insert(QStringLiteral("kind"), node.kind);
        item.insert(QStringLiteral("completed"), node.completed);
        item.insert(QStringLiteral("expanded"), node.expanded);
        item.insert(QStringLiteral("createdAt"), QString::number(node.created_at));
        item.insert(QStringLiteral("updatedAt"), QString::number(node.updated_at));
        nodes.append(item);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("nextId"), next_id_);
    root.insert(QStringLiteral("nodes"), nodes);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void ProjectTreeModel::load() {
    QFile file(storage_path_);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
    QVector<Node> loaded;
    int max_id = 0;
    for (const QJsonValue &value : nodes) {
        const QJsonObject item = value.toObject();
        Node node;
        node.id = item.value(QStringLiteral("id")).toInt();
        node.parent_id = item.value(QStringLiteral("parentId")).toInt();
        node.title = item.value(QStringLiteral("title")).toString().trimmed();
        node.kind = item.value(QStringLiteral("kind")).toString(QStringLiteral("task"));
        node.completed = item.value(QStringLiteral("completed")).toBool();
        node.expanded = item.value(QStringLiteral("expanded")).toBool(true);
        node.created_at = item.value(QStringLiteral("createdAt")).toString().toLongLong();
        node.updated_at = item.value(QStringLiteral("updatedAt")).toString().toLongLong();
        if (node.id <= 0 || node.title.isEmpty()) {
            continue;
        }
        loaded.append(node);
        max_id = std::max(max_id, node.id);
    }
    nodes_ = loaded;
    next_id_ = std::max(root.value(QStringLiteral("nextId")).toInt(max_id + 1), max_id + 1);
}

QString ProjectTreeModel::storagePath() const {
    const QString overrideDir = qEnvironmentVariable("STICKY_NOTES_DATA_DIR");
    const QString dataDir = overrideDir.trimmed().isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/data")
        : overrideDir;
    return QDir::toNativeSeparators(dataDir + QStringLiteral("/project-tree.json"));
}

int ProjectTreeModel::nextId() const {
    return next_id_;
}

void ProjectTreeModel::removeDescendants(int id) {
    for (int i = nodes_.size() - 1; i >= 0; --i) {
        if (isDescendantOf(nodes_.at(i).id, id)) {
            nodes_.removeAt(i);
        }
    }
}

bool ProjectTreeModel::isDescendantOf(int id, int parentId) const {
    int current_index = findNodeIndex(id);
    while (current_index >= 0) {
        const int current_parent = nodes_.at(current_index).parent_id;
        if (current_parent == parentId) {
            return true;
        }
        if (current_parent == 0) {
            return false;
        }
        current_index = findNodeIndex(current_parent);
    }
    return false;
}
