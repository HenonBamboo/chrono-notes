#include "project_tree_model.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

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
};

static void useDataDir(const QTemporaryDir &dir) {
    QVERIFY(dir.isValid());
    qputenv("STICKY_NOTES_DATA_DIR", QDir::toNativeSeparators(dir.path()).toUtf8());
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

QTEST_MAIN(ProjectTreeModelTests)

#include "project_tree_model_tests.moc"
