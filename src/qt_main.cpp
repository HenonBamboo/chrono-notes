#include "qt_note_app.h"
#include "local_profile.h"
#include "project_tree_model.h"
#include "workspace_recovery.h"

#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QColor>

#ifdef Q_OS_WIN
#include <shobjidl.h>
#endif

int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(L"com.chrononotes.desktop");
#endif
    QQuickWindow::setDefaultAlphaBuffer(true);
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("ChronoNotes"));
    app.setOrganizationDomain(QStringLiteral("chrononotes.local"));
    app.setApplicationName(QStringLiteral("ChronoNotes"));
    app.setApplicationVersion(QStringLiteral(CHRONONOTES_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/chrono_notes_logo.png")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    NoteApp note_app;
    ProjectTreeModel project_tree;
    const LocalProfilePaths profile = LocalProfile::resolve();
    WorkspaceRecovery workspace_recovery(
        profile.backupDir, QCoreApplication::applicationVersion());
    workspace_recovery.setSnapshotAccessors(
        [&note_app, &project_tree](
            BackupService::WorkspaceSnapshot *snapshot, QString *error) {
            if (!note_app.readWorkspaceSnapshot(snapshot, error)) {
                return false;
            }
            snapshot->projects = project_tree.exportSnapshot();
            return true;
        },
        [&note_app, &project_tree](
            const BackupService::WorkspaceSnapshot &snapshot, QString *error) {
            if (!note_app.applyWorkspaceSnapshot(snapshot, error)) {
                return false;
            }
            if (!project_tree.importSnapshot(snapshot.projects)) {
                if (error != nullptr) {
                    *error = project_tree.lastError().trimmed().isEmpty()
                                 ? QStringLiteral("项目树恢复失败。")
                                 : project_tree.lastError();
                }
                return false;
            }
            return true;
        });
    workspace_recovery.setDiagnosticsProvider(
        [&note_app, &project_tree, &workspace_recovery]() {
            QJsonObject result = note_app.diagnosticsSnapshot();
            result.insert(QStringLiteral("projectCount"),
                          project_tree.projectCount());
            result.insert(QStringLiteral("backupCount"),
                          workspace_recovery.availableBackups().size());
            return result;
        });
    workspace_recovery.ensureDailyBackup();

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("app"), QVariant::fromValue(&note_app)},
        {QStringLiteral("projectModel"), QVariant::fromValue(&project_tree)},
        {QStringLiteral("workspaceRecovery"),
         QVariant::fromValue(&workspace_recovery)}
    });
    engine.loadFromModule(QStringLiteral("ChronoNotes"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
        window->setColor(QColor(QStringLiteral("#F7F2E8")));
    }
    return app.exec();
}
