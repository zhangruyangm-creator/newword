#include "mainwindow.h"
#include "appstyle.h"
#include "docxconverter.h"
#include "documentrecovery.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QTimer>
#include <QtLogging>

int main(int argc, char *argv[])
{
    const bool profileStartup = qEnvironmentVariableIsSet("NEWWORD_STARTUP_PROFILE");
    QElapsedTimer boot;
    if (profileStartup)
        boot.start();

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NewWord"));
    QApplication::setOrganizationName(QStringLiteral("NewWord"));
    QApplication::setApplicationVersion(QStringLiteral("0.5"));

    QFont appFont = app.font();
#if defined(Q_OS_MACOS)
    appFont.setFamily(QStringLiteral("PingFang SC"));
#endif
    if (appFont.pointSize() < 12)
        appFont.setPointSize(12);
    app.setFont(appFont);
    AppStyle::loadThemeFromSettings();
    EditorDefaults::loadFromSettings();
    app.setStyleSheet(AppStyle::applicationStyleSheet());
    // Background python check so the first DOCX save/About never blocks the UI.
    DocxConverter::warmUpBridge();

    if (profileStartup)
        qInfo("startup: after QApplication+style %lld ms", static_cast<long long>(boot.elapsed()));

    MainWindow window;
    if (profileStartup)
        qInfo("startup: after MainWindow ctor %lld ms", static_cast<long long>(boot.elapsed()));

    window.show();
    if (profileStartup)
        qInfo("startup: after show() %lld ms", static_cast<long long>(boot.elapsed()));

    if (profileStartup) {
        QTimer::singleShot(0, &app, [&boot]() {
            qInfo("startup: first event-loop idle %lld ms", static_cast<long long>(boot.elapsed()));
        });
        QTimer::singleShot(50, &app, [&boot]() {
            qInfo("startup: +50ms after idle %lld ms", static_cast<long long>(boot.elapsed()));
        });
    }

    return app.exec();
}
