#include "project_tree_model.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

constexpr qint64 kMaximumProjectTreeBytes = 50LL * 1024LL * 1024LL;
constexpr int kMaximumProjectNodes = 100000;
constexpr int kMaximumTitleLength = 512;
constexpr int kMaximumDescriptionLength = 65536;

bool jsonInteger(const QJsonValue &value, int *result) {
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < std::numeric_limits<int>::min()
        || number > std::numeric_limits<int>::max()) {
        return false;
    }
    if (result != nullptr) {
        *result = static_cast<int>(number);
    }
    return true;
}

bool jsonTimestamp(const QJsonValue &value, qint64 *result) {
    bool ok = false;
    qint64 timestamp = 0;
    if (value.isString()) {
        timestamp = value.toString().toLongLong(&ok);
    } else if (value.isDouble()) {
        const double number = value.toDouble();
        ok = std::isfinite(number) && std::floor(number) == number
            && number >= 0.0
            && number <= static_cast<double>(std::numeric_limits<qint64>::max());
        if (ok) {
            timestamp = static_cast<qint64>(number);
        }
    }
    if (!ok || timestamp < 0) {
        return false;
    }
    if (result != nullptr) {
        *result = timestamp;
    }
    return true;
}

}

ProjectTreeModel::ProjectTreeModel(QObject *parent) : QAbstractListModel(parent) {
    storage_path_ = storagePath();
    load();
    rebuildCaches();
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
        case CompletedRole: {
            if (node.kind != QStringLiteral("task")) {
                return node.total_tasks > 0 && node.completed_tasks == node.total_tasks;
            }
            if (node.child_count > 0) {
                return node.total_tasks > 0 && node.completed_tasks == node.total_tasks;
            }
            return node.completed;
        }
        case DepthRole: return row.depth;
        case ExpandedRole: return node.expanded;
        case ChildCountRole: return node.child_count;
        case TotalTasksRole: return node.total_tasks;
        case CompletedTasksRole: return node.completed_tasks;
        case DescriptionRole: return node.description;
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
        {CompletedTasksRole, "completedTasks"},
        {DescriptionRole, "description"}
    };
}

int ProjectTreeModel::totalTasks() const {
    return total_tasks_;
}

int ProjectTreeModel::completedTasks() const {
    return completed_tasks_;
}

int ProjectTreeModel::projectCount() const {
    return project_count_;
}

QString ProjectTreeModel::lastError() const {
    return last_error_;
}

bool ProjectTreeModel::reloadFromDisk() {
    if (!load()) {
        return false;
    }
    resetToRows({});
    if (selected_node_id_ >= 0 && findNodeIndex(selected_node_id_) < 0) {
        selected_node_id_ = -1;
        emit selectionChanged();
    }
    return true;
}

QJsonObject ProjectTreeModel::exportSnapshot() const {
    QJsonArray nodes;
    for (const Node &node : nodes_) {
        QJsonObject item;
        item.insert(QStringLiteral("id"), node.id);
        item.insert(QStringLiteral("parentId"), node.parent_id);
        item.insert(QStringLiteral("title"), node.title);
        item.insert(QStringLiteral("kind"), node.kind);
        item.insert(QStringLiteral("description"), node.description);
        item.insert(QStringLiteral("completed"), node.completed);
        item.insert(QStringLiteral("expanded"), node.expanded);
        item.insert(QStringLiteral("createdAt"), QString::number(node.created_at));
        item.insert(QStringLiteral("updatedAt"), QString::number(node.updated_at));
        nodes.append(item);
    }
    QJsonObject snapshot;
    snapshot.insert(QStringLiteral("version"), 1);
    snapshot.insert(QStringLiteral("nextId"), next_id_);
    snapshot.insert(QStringLiteral("nodes"), nodes);
    return snapshot;
}

bool ProjectTreeModel::importSnapshot(const QJsonObject &snapshot) {
    QVector<Node> imported_nodes;
    int imported_next_id = 1;
    QString validation_error;
    if (!parseSnapshot(snapshot, &imported_nodes, &imported_next_id, &validation_error)) {
        setLastError(validation_error);
        return false;
    }

    const QVector<Node> previous_nodes = nodes_;
    const int previous_next_id = next_id_;
    const int previous_selection = selected_node_id_;
    nodes_ = imported_nodes;
    next_id_ = imported_next_id;
    if (!save()) {
        nodes_ = previous_nodes;
        next_id_ = previous_next_id;
        return false;
    }
    resetToRows({});
    if (selected_node_id_ >= 0 && findNodeIndex(selected_node_id_) < 0) {
        selected_node_id_ = -1;
    }
    if (selected_node_id_ != previous_selection) {
        emit selectionChanged();
    }
    return true;
}

int ProjectTreeModel::selectedNodeId() const {
    return selected_node_id_;
}

QString ProjectTreeModel::selectedNodeTitle() const {
    const int index = findNodeIndex(selected_node_id_);
    return index < 0 ? QString() : nodes_.at(index).title;
}

QString ProjectTreeModel::selectedNodeKind() const {
    const int index = findNodeIndex(selected_node_id_);
    return index < 0 ? QString() : nodes_.at(index).kind;
}

QString ProjectTreeModel::selectedNodeDescription() const {
    const int index = findNodeIndex(selected_node_id_);
    return index < 0 ? QString() : nodes_.at(index).description;
}

QStringList ProjectTreeModel::selectedAncestorPath() const {
    QStringList path = nodePath(selected_node_id_);
    if (!path.isEmpty()) {
        path.removeLast();
    }
    return path;
}

int ProjectTreeModel::selectedNodeChildCount() const {
    const int index = findNodeIndex(selected_node_id_);
    return index < 0 ? 0 : nodes_.at(index).child_count;
}

bool ProjectTreeModel::selectedNodeCompleted() const {
    const int index = findNodeIndex(selected_node_id_);
    return index >= 0 && nodes_.at(index).completed;
}

QStringList ProjectTreeModel::nodePath(int id) const {
    QStringList path;
    QSet<int> visited;
    int index = findNodeIndex(id);
    while (index >= 0) {
        const Node &node = nodes_.at(index);
        if (visited.contains(node.id)) {
            return {};
        }
        visited.insert(node.id);
        path.prepend(node.title);
        if (node.parent_id == 0) {
            return path;
        }
        index = findNodeIndex(node.parent_id);
    }
    return {};
}

bool ProjectTreeModel::selectNode(int id) {
    if (id != -1 && findNodeIndex(id) < 0) {
        return false;
    }
    if (selected_node_id_ == id) {
        return true;
    }
    selected_node_id_ = id;
    emit selectionChanged();
    return true;
}

int ProjectTreeModel::addProject(const QString &title) {
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }
    const QVector<Node> previous_nodes = nodes_;
    const int previous_next_id = next_id_;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const int id = next_id_++;
    nodes_.append(Node{id, 0, trimmed, QStringLiteral("project"), QString(), false, true, now, now});
    if (!save()) {
        nodes_ = previous_nodes;
        next_id_ = previous_next_id;
        return -1;
    }
    resetToRows({});
    return id;
}

int ProjectTreeModel::addChild(int parentId, const QString &title) {
    const int parent_index = findNodeIndex(parentId);
    const QString trimmed = title.trimmed();
    if (parent_index < 0 || trimmed.isEmpty()) {
        return -1;
    }
    const QVector<Node> previous_nodes = nodes_;
    const int previous_next_id = next_id_;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const int id = next_id_++;
    nodes_[parent_index].expanded = true;
    nodes_[parent_index].updated_at = now;
    nodes_.append(Node{id, parentId, trimmed, QStringLiteral("task"), QString(), false, true, now, now});
    if (!save()) {
        nodes_ = previous_nodes;
        next_id_ = previous_next_id;
        return -1;
    }
    resetToRows({});
    return id;
}

bool ProjectTreeModel::updateTitle(int id, const QString &title) {
    const int index = findNodeIndex(id);
    const QString trimmed = title.trimmed();
    if (index < 0 || trimmed.isEmpty() || nodes_[index].title == trimmed) {
        return false;
    }
    const Node previous = nodes_.at(index);
    nodes_[index].title = trimmed;
    nodes_[index].updated_at = QDateTime::currentSecsSinceEpoch();
    if (!save()) {
        nodes_[index] = previous;
        return false;
    }
    resetToRows({});
    if (selected_node_id_ == id) {
        emit selectionChanged();
    }
    return true;
}

bool ProjectTreeModel::updateDescription(int id, const QString &description) {
    const int index = findNodeIndex(id);
    if (index < 0 || nodes_[index].description == description) {
        return false;
    }
    const Node previous = nodes_.at(index);
    nodes_[index].description = description;
    nodes_[index].updated_at = QDateTime::currentSecsSinceEpoch();
    if (!save()) {
        nodes_[index] = previous;
        return false;
    }
    resetToRows({});
    if (selected_node_id_ == id) {
        emit selectionChanged();
    }
    return true;
}

bool ProjectTreeModel::toggleComplete(int id) {
    const int index = findNodeIndex(id);
    if (index < 0 || nodes_[index].kind == QStringLiteral("project") || hasChildren(id)) {
        return false;
    }
    const Node previous = nodes_.at(index);
    nodes_[index].completed = !nodes_[index].completed;
    nodes_[index].updated_at = QDateTime::currentSecsSinceEpoch();
    if (!save()) {
        nodes_[index] = previous;
        return false;
    }
    resetToRows({});
    return true;
}

bool ProjectTreeModel::toggleExpanded(int id) {
    const int index = findNodeIndex(id);
    if (index < 0 || !hasChildren(id)) {
        return false;
    }
    const Node previous = nodes_.at(index);
    nodes_[index].expanded = !nodes_[index].expanded;
    if (!save()) {
        nodes_[index] = previous;
        return false;
    }
    resetToRows({});
    return true;
}

bool ProjectTreeModel::removeNode(int id) {
    const int index = findNodeIndex(id);
    if (index < 0) {
        return false;
    }
    const bool removes_selection = selected_node_id_ == id
        || isDescendantOf(selected_node_id_, id);
    const QVector<Node> previous_nodes = nodes_;
    removeDescendants(id);
    const auto target = std::find_if(nodes_.begin(), nodes_.end(), [id](const Node &node) {
        return node.id == id;
    });
    if (target != nodes_.end()) {
        nodes_.erase(target);
    }
    if (!save()) {
        nodes_ = previous_nodes;
        return false;
    }
    resetToRows({});
    if (removes_selection) {
        selected_node_id_ = -1;
        emit selectionChanged();
    }
    return true;
}

int ProjectTreeModel::findNodeIndex(int id) const {
    return node_index_by_id_.value(id, -1);
}

bool ProjectTreeModel::hasChildren(int id) const {
    return directChildCount(id) > 0;
}

int ProjectTreeModel::directChildCount(int id) const {
    const int index = findNodeIndex(id);
    return index < 0 ? 0 : nodes_.at(index).child_count;
}

int ProjectTreeModel::descendantTaskCount(int id) const {
    const int index = findNodeIndex(id);
    if (index < 0) {
        return 0;
    }
    return nodes_.at(index).total_tasks;
}

int ProjectTreeModel::completedDescendantTaskCount(int id) const {
    const int index = findNodeIndex(id);
    if (index < 0) {
        return 0;
    }
    return nodes_.at(index).completed_tasks;
}

void ProjectTreeModel::appendVisibleRows(int parentId, int depth, QVector<Row> *rows) const {
    if (rows == nullptr) {
        return;
    }
    const QVector<int> children = children_by_parent_.value(parentId);
    for (const int index : children) {
        const Node &node = nodes_.at(index);
        rows->append(Row{index, depth});
        if (node.expanded) {
            appendVisibleRows(node.id, depth + 1, rows);
        }
    }
}

void ProjectTreeModel::rebuildCaches() {
    node_index_by_id_.clear();
    children_by_parent_.clear();
    total_tasks_ = 0;
    completed_tasks_ = 0;
    project_count_ = 0;

    for (int i = 0; i < nodes_.size(); ++i) {
        Node &node = nodes_[i];
        node.child_count = 0;
        node.total_tasks = 0;
        node.completed_tasks = 0;
        node_index_by_id_.insert(node.id, i);
        children_by_parent_[node.parent_id].append(i);
        if (node.parent_id == 0) {
            ++project_count_;
        }
    }
    for (Node &node : nodes_) {
        node.child_count = children_by_parent_.value(node.id).size();
    }

    QVector<QPair<int, bool>> stack;
    for (int i = 0; i < nodes_.size(); ++i) {
        if (nodes_.at(i).parent_id == 0) {
            stack.append({i, false});
        }
    }
    while (!stack.isEmpty()) {
        const auto [index, children_processed] = stack.takeLast();
        Node &node = nodes_[index];
        if (!children_processed) {
            stack.append({index, true});
            for (const int child_index : children_by_parent_.value(node.id)) {
                stack.append({child_index, false});
            }
            continue;
        }
        if (node.kind == QStringLiteral("task") && node.child_count == 0) {
            node.total_tasks = 1;
            node.completed_tasks = node.completed ? 1 : 0;
        } else {
            for (const int child_index : children_by_parent_.value(node.id)) {
                node.total_tasks += nodes_.at(child_index).total_tasks;
                node.completed_tasks += nodes_.at(child_index).completed_tasks;
            }
        }
    }

    for (const Node &node : nodes_) {
        if (node.kind == QStringLiteral("task") && node.child_count == 0) {
            ++total_tasks_;
            if (node.completed) {
                ++completed_tasks_;
            }
        }
    }
}

void ProjectTreeModel::rebuildRows() {
    rows_.clear();
    appendVisibleRows(0, 0, &rows_);
}

void ProjectTreeModel::resetToRows(QVector<Row> rows) {
    beginResetModel();
    rebuildCaches();
    if (rows.isEmpty()) {
        rebuildRows();
    } else {
        rows_ = rows;
    }
    endResetModel();
    emit treeChanged();
}

bool ProjectTreeModel::save() {
    const QString parent_path = QFileInfo(storage_path_).absolutePath();
    if (!QDir(parent_path).exists() && !QDir().mkpath(parent_path)) {
        setLastError(QStringLiteral("无法创建项目树目录。"));
        return false;
    }
    const QByteArray bytes = QJsonDocument(exportSnapshot()).toJson(QJsonDocument::Indented);

    QSaveFile file(storage_path_);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastError(QStringLiteral("无法写入项目树：%1").arg(file.errorString()));
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        setLastError(QStringLiteral("项目树写入不完整：%1").arg(file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setLastError(QStringLiteral("无法原子提交项目树：%1").arg(file.errorString()));
        return false;
    }
    setLastError(QString(), false);
    return true;
}

bool ProjectTreeModel::parseSnapshot(const QJsonObject &root, QVector<Node> *parsed_nodes,
                                     int *parsed_next_id, QString *error) const {
    auto reject = [error](const QString &message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (parsed_nodes == nullptr || parsed_next_id == nullptr) {
        return reject(QStringLiteral("项目树导入目标不可用。"));
    }
    int version = 0;
    if (!jsonInteger(root.value(QStringLiteral("version")), &version) || version != 1) {
        return reject(QStringLiteral("不支持的项目树版本。"));
    }
    if (!root.value(QStringLiteral("nodes")).isArray()) {
        return reject(QStringLiteral("项目树缺少 nodes 数组。"));
    }
    const QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
    if (nodes.size() > kMaximumProjectNodes) {
        return reject(QStringLiteral("项目树节点数量超过安全上限。"));
    }
    QVector<Node> loaded;
    loaded.reserve(nodes.size());
    QSet<int> ids;
    int max_id = 0;
    for (const QJsonValue &value : nodes) {
        if (!value.isObject()) {
            return reject(QStringLiteral("项目树节点必须是 JSON 对象。"));
        }
        const QJsonObject item = value.toObject();
        Node node;
        if (!jsonInteger(item.value(QStringLiteral("id")), &node.id) || node.id <= 0
            || node.id == std::numeric_limits<int>::max()
            || !jsonInteger(item.value(QStringLiteral("parentId")), &node.parent_id)
            || node.parent_id < 0) {
            return reject(QStringLiteral("项目树节点 ID 或 parentId 无效。"));
        }
        if (!item.value(QStringLiteral("title")).isString()) {
            return reject(QStringLiteral("项目树节点标题必须是字符串。"));
        }
        node.title = item.value(QStringLiteral("title")).toString().trimmed();
        if (node.title.isEmpty() || node.title.size() > kMaximumTitleLength) {
            return reject(QStringLiteral("项目树节点标题为空或过长。"));
        }
        if (!item.value(QStringLiteral("kind")).isString()) {
            return reject(QStringLiteral("项目树节点 kind 必须是字符串。"));
        }
        node.kind = item.value(QStringLiteral("kind")).toString();
        if (node.kind != QStringLiteral("project") && node.kind != QStringLiteral("task")) {
            return reject(QStringLiteral("项目树节点 kind 无效。"));
        }
        if ((node.parent_id == 0) != (node.kind == QStringLiteral("project"))) {
            return reject(QStringLiteral("只有 project 节点可以位于根级。"));
        }
        const QJsonValue description = item.value(QStringLiteral("description"));
        if (!description.isUndefined() && !description.isString()) {
            return reject(QStringLiteral("项目树节点 description 必须是字符串。"));
        }
        node.description = description.toString();
        if (node.description.size() > kMaximumDescriptionLength) {
            return reject(QStringLiteral("项目树节点 description 过长。"));
        }
        if (!item.value(QStringLiteral("completed")).isBool()
            || !item.value(QStringLiteral("expanded")).isBool()) {
            return reject(QStringLiteral("项目树节点状态字段无效。"));
        }
        node.completed = item.value(QStringLiteral("completed")).toBool();
        node.expanded = item.value(QStringLiteral("expanded")).toBool();
        if (!jsonTimestamp(item.value(QStringLiteral("createdAt")), &node.created_at)
            || !jsonTimestamp(item.value(QStringLiteral("updatedAt")), &node.updated_at)) {
            return reject(QStringLiteral("项目树节点时间字段无效。"));
        }
        if (ids.contains(node.id)) {
            return reject(QStringLiteral("项目树包含重复节点 ID：%1。").arg(node.id));
        }
        ids.insert(node.id);
        loaded.append(node);
        max_id = std::max(max_id, node.id);
    }

    QHash<int, int> parents;
    for (const Node &node : loaded) {
        parents.insert(node.id, node.parent_id);
        if (node.parent_id != 0 && !ids.contains(node.parent_id)) {
            return reject(QStringLiteral("项目树包含孤立节点：%1。").arg(node.id));
        }
    }
    QHash<int, int> state;
    for (const int id : std::as_const(ids)) {
        QVector<int> path;
        int current = id;
        while (current != 0 && state.value(current, 0) != 2) {
            if (state.value(current, 0) == 1) {
                return reject(QStringLiteral("项目树包含循环引用。"));
            }
            state.insert(current, 1);
            path.append(current);
            current = parents.value(current, 0);
        }
        for (const int visited : std::as_const(path)) {
            state.insert(visited, 2);
        }
    }

    int requested_next_id = max_id + 1;
    const QJsonValue next_id_value = root.value(QStringLiteral("nextId"));
    if (!next_id_value.isUndefined()) {
        if (!jsonInteger(next_id_value, &requested_next_id) || requested_next_id <= max_id) {
            return reject(QStringLiteral("项目树 nextId 无效。"));
        }
    }
    *parsed_nodes = loaded;
    *parsed_next_id = requested_next_id;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool ProjectTreeModel::load() {
    QFile file(storage_path_);
    if (!file.open(QIODevice::ReadOnly)) {
        if (!file.exists()) {
            setLastError(QString(), false);
            return true;
        }
        setLastError(QStringLiteral("无法读取项目树文件：%1").arg(file.errorString()));
        return false;
    }
    if (file.size() < 0 || file.size() > kMaximumProjectTreeBytes) {
        setLastError(QStringLiteral("项目树文件超过 50 MiB 安全上限。"));
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError || bytes.size() != file.size()) {
        setLastError(QStringLiteral("项目树文件读取不完整：%1").arg(file.errorString()));
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parse_error);
    if (!doc.isObject()) {
        setLastError(QStringLiteral("项目树 JSON 无效：%1").arg(parse_error.errorString()));
        return false;
    }
    QVector<Node> loaded;
    int requested_next_id = 1;
    QString validation_error;
    if (!parseSnapshot(doc.object(), &loaded, &requested_next_id, &validation_error)) {
        setLastError(validation_error);
        return false;
    }
    nodes_ = loaded;
    next_id_ = requested_next_id;
    setLastError(QString(), false);
    return true;
}

void ProjectTreeModel::setLastError(const QString &message, bool emitFailure) {
    if (last_error_ != message) {
        last_error_ = message;
        emit errorChanged();
    }
    if (emitFailure && !message.isEmpty()) {
        emit persistenceError(message);
    }
}

QString ProjectTreeModel::storagePath() const {
    const QString overrideDir = qEnvironmentVariable("STICKY_NOTES_DATA_DIR").trimmed();
    const QString applicationDir = QCoreApplication::applicationDirPath();
    QString dataDir;
    if (!overrideDir.isEmpty()) {
        dataDir = overrideDir;
    } else if (QFileInfo::exists(QDir(applicationDir).filePath(QStringLiteral("portable.flag")))) {
        dataDir = QDir(applicationDir).filePath(QStringLiteral("data"));
    } else {
        dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    return QDir::toNativeSeparators(QDir(dataDir).filePath(QStringLiteral("project-tree.json")));
}

int ProjectTreeModel::nextId() const {
    return next_id_;
}

void ProjectTreeModel::removeDescendants(int id) {
    QSet<int> descendant_ids;
    for (const Node &node : std::as_const(nodes_)) {
        if (isDescendantOf(node.id, id)) {
            descendant_ids.insert(node.id);
        }
    }
    for (int i = nodes_.size() - 1; i >= 0; --i) {
        if (descendant_ids.contains(nodes_.at(i).id)) {
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
