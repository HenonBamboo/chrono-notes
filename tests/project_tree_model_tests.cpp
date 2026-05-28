#include "project_tree_model.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

class ProjectTreeModelTests : public QObject {
    Q_OBJECT

private slots:
    void cleanup();
    void createsAndBreaksDownProjects();
    void completionRollsUpToParentStats();
    void collapsingParentHidesChildren();
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
