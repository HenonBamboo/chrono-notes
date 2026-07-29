#include "project_tree_model.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

class ProjectTreeModelTests : public QObject {
    Q_OBJECT

private slots:
    void cleanup();
    void createsAndBreaksDownProjects();
    void completionRollsUpToParentStats();
    void parentTasksUseChildrenForProgressOnly();
    void parentTasksCannotBeToggledDirectly();
    void collapsingParentHidesChildren();
    void descriptionsAreEditableAndPersisted();
    void legacyProjectTreeWithoutDescriptionLoadsEmptyDescription();
    void supportsDeepTaskHierarchy();
    void persistsTreeInDataDirectory();
    void rejectsDuplicateIdsWithoutReplacingCurrentTree();
    void rejectsInvalidTopologyAndSchema_data();
    void rejectsInvalidTopologyAndSchema();
    void saveFailureRollsBackMutationAndReportsError();
    void exposesSelectedNodeAndItsRealAncestorPath();
    void workspaceSnapshotImportIsValidatedPersistedAndRolledBack();
    void fiveThousandNodeOperationsStayUnderThreshold();
};

static void useDataDir(const QTemporaryDir &dir) {
    QVERIFY(dir.isValid());
    qputenv("STICKY_NOTES_DATA_DIR", QDir::toNativeSeparators(dir.path()).toUtf8());
}

static QJsonObject projectNode(int id, const QString &title = QStringLiteral("Project")) {
    QJsonObject item;
    item.insert(QStringLiteral("id"), id);
    item.insert(QStringLiteral("parentId"), 0);
    item.insert(QStringLiteral("title"), title);
    item.insert(QStringLiteral("kind"), QStringLiteral("project"));
    item.insert(QStringLiteral("completed"), false);
    item.insert(QStringLiteral("expanded"), true);
    item.insert(QStringLiteral("createdAt"), QStringLiteral("1"));
    item.insert(QStringLiteral("updatedAt"), QStringLiteral("1"));
    return item;
}

static QJsonObject taskNode(int id, int parentId, const QString &title = QStringLiteral("Task")) {
    QJsonObject item = projectNode(id, title);
    item.insert(QStringLiteral("parentId"), parentId);
    item.insert(QStringLiteral("kind"), QStringLiteral("task"));
    return item;
}

void ProjectTreeModelTests::cleanup() {
    qunsetenv("STICKY_NOTES_DATA_DIR");
}

void ProjectTreeModelTests::createsAndBreaksDownProjects() {
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;

    const int project_id = model.addProject(QStringLiteral("发布新版"));
    const int task_id = model.addChild(project_id, QStringLiteral("整理验收清单"));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::TitleRole).toString(), QStringLiteral("发布新版"));
    QCOMPARE(model.data(model.index(1, 0), ProjectTreeModel::ParentIdRole).toInt(), project_id);
    QCOMPARE(model.data(model.index(1, 0), ProjectTreeModel::DepthRole).toInt(), 1);
    QVERIFY(task_id > project_id);
}

void ProjectTreeModelTests::completionRollsUpToParentStats() {
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;

    const int project_id = model.addProject(QStringLiteral("重构"));
    const int first_id = model.addChild(project_id, QStringLiteral("拆模型"));
    model.addChild(project_id, QStringLiteral("接界面"));

    model.toggleComplete(first_id);

    QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::CompletedTasksRole).toInt(), 1);
    QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::TotalTasksRole).toInt(), 2);
    QCOMPARE(model.data(model.index(1, 0), ProjectTreeModel::CompletedRole).toBool(), true);
}

void ProjectTreeModelTests::parentTasksUseChildrenForProgressOnly() {
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;

    const int project_id = model.addProject(QStringLiteral("项目"));
    const int parent_task_id = model.addChild(project_id, QStringLiteral("父任务"));
    const int first_child_id = model.addChild(parent_task_id, QStringLiteral("子任务一"));
    model.addChild(parent_task_id, QStringLiteral("子任务二"));

    QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::TotalTasksRole).toInt(), 2);
    QCOMPARE(model.data(model.index(1, 0), ProjectTreeModel::TotalTasksRole).toInt(), 2);
    QCOMPARE(model.totalTasks(), 2);

    model.toggleComplete(first_child_id);

    QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::CompletedTasksRole).toInt(), 1);
    QCOMPARE(model.data(model.index(1, 0), ProjectTreeModel::CompletedTasksRole).toInt(), 1);
    QCOMPARE(model.completedTasks(), 1);
}

void ProjectTreeModelTests::parentTasksCannotBeToggledDirectly() {
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;

    const int project_id = model.addProject(QStringLiteral("项目"));
    const int parent_task_id = model.addChild(project_id, QStringLiteral("父任务"));
    const int first_child_id = model.addChild(parent_task_id, QStringLiteral("子任务一"));
    const int second_child_id = model.addChild(parent_task_id, QStringLiteral("子任务二"));

    QVERIFY(!model.toggleComplete(parent_task_id));
    QCOMPARE(model.data(model.index(1, 0), ProjectTreeModel::CompletedRole).toBool(), false);

    QVERIFY(model.toggleComplete(first_child_id));
    QCOMPARE(model.data(model.index(1, 0), ProjectTreeModel::CompletedRole).toBool(), false);

    QVERIFY(model.toggleComplete(second_child_id));
    QCOMPARE(model.data(model.index(1, 0), ProjectTreeModel::CompletedRole).toBool(), true);
}

void ProjectTreeModelTests::collapsingParentHidesChildren() {
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;

    const int project_id = model.addProject(QStringLiteral("项目"));
    model.addChild(project_id, QStringLiteral("任务"));

    QCOMPARE(model.rowCount(), 2);
    model.toggleExpanded(project_id);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::ExpandedRole).toBool(), false);
}

void ProjectTreeModelTests::descriptionsAreEditableAndPersisted() {
    QTemporaryDir dir;
    useDataDir(dir);

    int task_id = 0;
    {
        ProjectTreeModel model;
        const int project_id = model.addProject(QStringLiteral("产品改版"));
        task_id = model.addChild(project_id, QStringLiteral("梳理交互"));
        QVERIFY(model.updateDescription(project_id, QStringLiteral("项目背景和验收要求")));
        QVERIFY(model.updateDescription(task_id, QStringLiteral("记录任务的具体处理说明")));

        QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::DescriptionRole).toString(), QStringLiteral("项目背景和验收要求"));
        QCOMPARE(model.data(model.index(1, 0), ProjectTreeModel::DescriptionRole).toString(), QStringLiteral("记录任务的具体处理说明"));
    }

    ProjectTreeModel reloaded;
    QCOMPARE(reloaded.data(reloaded.index(0, 0), ProjectTreeModel::DescriptionRole).toString(), QStringLiteral("项目背景和验收要求"));
    QCOMPARE(reloaded.data(reloaded.index(1, 0), ProjectTreeModel::DescriptionRole).toString(), QStringLiteral("记录任务的具体处理说明"));
    QVERIFY(task_id > 0);
}

void ProjectTreeModelTests::legacyProjectTreeWithoutDescriptionLoadsEmptyDescription() {
    QTemporaryDir dir;
    useDataDir(dir);

    QJsonObject item;
    item.insert(QStringLiteral("id"), 1);
    item.insert(QStringLiteral("parentId"), 0);
    item.insert(QStringLiteral("title"), QStringLiteral("旧项目"));
    item.insert(QStringLiteral("kind"), QStringLiteral("project"));
    item.insert(QStringLiteral("completed"), false);
    item.insert(QStringLiteral("expanded"), true);
    item.insert(QStringLiteral("createdAt"), QStringLiteral("1"));
    item.insert(QStringLiteral("updatedAt"), QStringLiteral("1"));

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("nextId"), 2);
    root.insert(QStringLiteral("nodes"), QJsonArray{item});

    QFile file(dir.filePath(QStringLiteral("project-tree.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(root).toJson());
    file.close();

    ProjectTreeModel model;
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::DescriptionRole).toString(), QString());
}

void ProjectTreeModelTests::supportsDeepTaskHierarchy() {
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;

    int parent_id = model.addProject(QStringLiteral("深层项目"));
    for (int depth = 1; depth <= 8; ++depth) {
        parent_id = model.addChild(parent_id, QStringLiteral("第%1层任务").arg(depth));
        QVERIFY(parent_id > 0);
    }

    QCOMPARE(model.rowCount(), 9);
    QCOMPARE(model.data(model.index(8, 0), ProjectTreeModel::DepthRole).toInt(), 8);
}

void ProjectTreeModelTests::persistsTreeInDataDirectory() {
    QTemporaryDir dir;
    useDataDir(dir);

    {
        ProjectTreeModel model;
        const int project_id = model.addProject(QStringLiteral("可持久化项目"));
        model.addChild(project_id, QStringLiteral("可持久化任务"));
    }

    ProjectTreeModel reloaded;
    QCOMPARE(reloaded.rowCount(), 2);
    QCOMPARE(reloaded.data(reloaded.index(0, 0), ProjectTreeModel::TitleRole).toString(), QStringLiteral("可持久化项目"));
    QCOMPARE(reloaded.data(reloaded.index(1, 0), ProjectTreeModel::DepthRole).toInt(), 1);
}

void ProjectTreeModelTests::rejectsDuplicateIdsWithoutReplacingCurrentTree() {
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;
    model.addProject(QStringLiteral("Existing project"));

    QJsonObject first;
    first.insert(QStringLiteral("id"), 1);
    first.insert(QStringLiteral("parentId"), 0);
    first.insert(QStringLiteral("title"), QStringLiteral("First"));
    first.insert(QStringLiteral("kind"), QStringLiteral("project"));
    first.insert(QStringLiteral("completed"), false);
    first.insert(QStringLiteral("expanded"), true);
    first.insert(QStringLiteral("createdAt"), QStringLiteral("1"));
    first.insert(QStringLiteral("updatedAt"), QStringLiteral("1"));
    QJsonObject duplicate = first;
    duplicate.insert(QStringLiteral("title"), QStringLiteral("Duplicate"));

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("nextId"), 2);
    root.insert(QStringLiteral("nodes"), QJsonArray{first, duplicate});
    QFile file(dir.filePath(QStringLiteral("project-tree.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(QJsonDocument(root).toJson()), QJsonDocument(root).toJson().size());
    file.close();

    QVERIFY(!model.reloadFromDisk());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::TitleRole).toString(),
             QStringLiteral("Existing project"));
    QVERIFY(!model.lastError().isEmpty());
}

void ProjectTreeModelTests::rejectsInvalidTopologyAndSchema_data() {
    QTest::addColumn<QJsonObject>("document");

    auto document = [](const QJsonValue &version, const QJsonArray &nodes) {
        QJsonObject root;
        root.insert(QStringLiteral("version"), version);
        root.insert(QStringLiteral("nextId"), 10);
        root.insert(QStringLiteral("nodes"), nodes);
        return root;
    };

    QTest::newRow("unsupported-version") << document(2, QJsonArray{projectNode(1)});
    QTest::newRow("orphan") << document(1, QJsonArray{projectNode(1), taskNode(2, 99)});
    QTest::newRow("cycle") << document(1, QJsonArray{taskNode(1, 2), taskNode(2, 1)});

    QJsonObject invalidKind = taskNode(2, 1);
    invalidKind.insert(QStringLiteral("kind"), QStringLiteral("milestone"));
    QTest::newRow("invalid-kind") << document(1, QJsonArray{projectNode(1), invalidKind});
    QTest::newRow("blank-title") << document(1, QJsonArray{projectNode(1, QStringLiteral("   "))});

    QJsonObject rootTask = taskNode(1, 0);
    QTest::newRow("root-task") << document(1, QJsonArray{rootTask});
}

void ProjectTreeModelTests::rejectsInvalidTopologyAndSchema() {
    QFETCH(QJsonObject, document);
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;
    QVERIFY(model.addProject(QStringLiteral("Existing project")) > 0);

    QFile file(dir.filePath(QStringLiteral("project-tree.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray bytes = QJsonDocument(document).toJson();
    QCOMPARE(file.write(bytes), bytes.size());
    file.close();

    QVERIFY(!model.reloadFromDisk());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ProjectTreeModel::TitleRole).toString(),
             QStringLiteral("Existing project"));
    QVERIFY(!model.lastError().isEmpty());
}

void ProjectTreeModelTests::saveFailureRollsBackMutationAndReportsError() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString blockerPath = dir.filePath(QStringLiteral("not-a-directory"));
    QFile blocker(blockerPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.write("block");
    blocker.close();
    qputenv("STICKY_NOTES_DATA_DIR", QDir::toNativeSeparators(blockerPath).toUtf8());

    ProjectTreeModel model;
    QSignalSpy failureSpy(&model, &ProjectTreeModel::persistenceError);

    QCOMPARE(model.addProject(QStringLiteral("Must not remain in memory")), -1);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.projectCount(), 0);
    QCOMPARE(failureSpy.count(), 1);
    QVERIFY(!model.lastError().isEmpty());
}

void ProjectTreeModelTests::exposesSelectedNodeAndItsRealAncestorPath() {
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;
    const int projectId = model.addProject(QStringLiteral("Release"));
    const int parentTaskId = model.addChild(projectId, QStringLiteral("Quality"));
    const int leafTaskId = model.addChild(parentTaskId, QStringLiteral("Regression"));
    QVERIFY(model.updateDescription(leafTaskId, QStringLiteral("Run the full suite")));

    QVERIFY(model.selectNode(leafTaskId));
    QCOMPARE(model.selectedNodeId(), leafTaskId);
    QCOMPARE(model.selectedNodeTitle(), QStringLiteral("Regression"));
    QCOMPARE(model.selectedNodeKind(), QStringLiteral("task"));
    QCOMPARE(model.selectedNodeDescription(), QStringLiteral("Run the full suite"));
    QCOMPARE(model.selectedNodeChildCount(), 0);
    QCOMPARE(model.selectedNodeCompleted(), false);
    QCOMPARE(model.selectedAncestorPath(),
             QStringList({QStringLiteral("Release"), QStringLiteral("Quality")}));
    QCOMPARE(model.nodePath(leafTaskId),
             QStringList({QStringLiteral("Release"), QStringLiteral("Quality"),
                          QStringLiteral("Regression")}));
    QVERIFY(model.toggleComplete(leafTaskId));
    QCOMPARE(model.selectedNodeCompleted(), true);

    QVERIFY(model.selectNode(parentTaskId));
    QCOMPARE(model.selectedNodeChildCount(), 1);
    QCOMPARE(model.selectedNodeCompleted(), false);

    QVERIFY(model.removeNode(parentTaskId));
    QCOMPARE(model.selectedNodeId(), -1);
    QVERIFY(model.selectedAncestorPath().isEmpty());
}

void ProjectTreeModelTests::workspaceSnapshotImportIsValidatedPersistedAndRolledBack() {
    QTemporaryDir sourceDir;
    useDataDir(sourceDir);
    ProjectTreeModel source;
    const int projectId = source.addProject(QStringLiteral("Workspace project"));
    source.addChild(projectId, QStringLiteral("Workspace task"));
    const QJsonObject snapshot = source.exportSnapshot();

    QTemporaryDir destinationDir;
    useDataDir(destinationDir);
    ProjectTreeModel destination;
    QVERIFY(destination.importSnapshot(snapshot));
    QCOMPARE(destination.rowCount(), 2);

    QJsonObject invalid = snapshot;
    QJsonArray nodes = invalid.value(QStringLiteral("nodes")).toArray();
    nodes.append(nodes.first());
    invalid.insert(QStringLiteral("nodes"), nodes);
    QVERIFY(!destination.importSnapshot(invalid));
    QCOMPARE(destination.rowCount(), 2);
    QCOMPARE(destination.data(destination.index(0, 0), ProjectTreeModel::TitleRole).toString(),
             QStringLiteral("Workspace project"));

    ProjectTreeModel reloaded;
    QCOMPARE(reloaded.rowCount(), 2);
    QCOMPARE(reloaded.exportSnapshot(), snapshot);

    QTemporaryDir blockedDir;
    const QString blockerPath = blockedDir.filePath(QStringLiteral("not-a-directory"));
    QFile blocker(blockerPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.write("block");
    blocker.close();
    qputenv("STICKY_NOTES_DATA_DIR", QDir::toNativeSeparators(blockerPath).toUtf8());
    ProjectTreeModel blocked;
    QVERIFY(!blocked.importSnapshot(snapshot));
    QCOMPARE(blocked.rowCount(), 0);
}

void ProjectTreeModelTests::fiveThousandNodeOperationsStayUnderThreshold() {
    QTemporaryDir dir;
    useDataDir(dir);
    ProjectTreeModel model;

    QJsonArray nodes;
    nodes.append(projectNode(1, QStringLiteral("Large project")));
    for (int id = 2; id <= 5'000; ++id) {
        nodes.append(taskNode(id, 1, QStringLiteral("Task %1").arg(id)));
    }
    QJsonObject snapshot;
    snapshot.insert(QStringLiteral("version"), 1);
    snapshot.insert(QStringLiteral("nextId"), 5'001);
    snapshot.insert(QStringLiteral("nodes"), nodes);
    QVERIFY2(model.importSnapshot(snapshot), qPrintable(model.lastError()));
    QCOMPARE(model.rowCount(), 5'000);
    QCOMPARE(model.totalTasks(), 4'999);

    QList<qint64> samples;
    for (int iteration = 0; iteration < 25; ++iteration) {
        const int id = 2 + (iteration * 197) % 4'999;
        QElapsedTimer timer;
        timer.start();
        QVERIFY(model.selectNode(id));
        QCOMPARE(model.selectedAncestorPath(),
                 QStringList{QStringLiteral("Large project")});
        QCOMPARE(model.nodePath(id).size(), 2);
        QCOMPARE(model.data(model.index(0, 0),
                            ProjectTreeModel::TotalTasksRole)
                     .toInt(),
                 4'999);
        samples.append(timer.nsecsElapsed() / 1'000'000);
    }
    std::sort(samples.begin(), samples.end());
    const qint64 p95 = samples.at((samples.size() * 95 + 99) / 100 - 1);
    qInfo().noquote()
        << QStringLiteral("5,000 node tree operation p95=%1 ms").arg(p95);
    QVERIFY2(p95 <= 100,
             qPrintable(QStringLiteral(
                            "5,000 node tree operation p95=%1 ms exceeds 100 ms")
                            .arg(p95)));
}

QTEST_MAIN(ProjectTreeModelTests)

#include "project_tree_model_tests.moc"
