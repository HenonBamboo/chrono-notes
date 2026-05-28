#include "qt_note_app.h"
#include "project_tree_model.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QColor>

int main(int argc, char *argv[]) {
    QQuickWindow::setDefaultAlphaBuffer(true);
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Codex"));
    app.setApplicationName(QStringLiteral("ChronoNotes"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/chrono_notes_logo.png")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    NoteApp note_app;
    ProjectTreeModel project_tree;

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("app"), QVariant::fromValue(&note_app)},
        {QStringLiteral("projectModel"), QVariant::fromValue(&project_tree)}
    });
    engine.loadFromModule(QStringLiteral("ChronoNotes"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
        window->setColor(QColor(QStringLiteral("#fff5af")));
        window->setFlags(window->flags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    }
    return app.exec();
}
