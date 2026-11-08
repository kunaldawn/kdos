/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-installer-gui — install KDOS, in a window
 *
 * A Kirigami front end over kinstall: it collects the answers page by page,
 * shows what kinstall says they would do, and runs the install through
 * pkexec, reading its event stream. See backend.h.
 */

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kdos-installer-gui");

    KAboutData about(QStringLiteral("kdos-installer-gui"),
                     i18nc("@title the application's name", "Install KDOS"),
                     QStringLiteral(KDOS_INSTALLER_VERSION),
                     i18n("Install KDOS on this computer's disk"),
                     KAboutLicense::MIT);
    about.setHomepage(QStringLiteral("https://github.com/kunaldawn/kdos"));
    about.setDesktopFileName(QStringLiteral("kdos-installer"));
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("system-software-install")));

    /* The desktop style, so the controls are the session's own Breeze and
     * follow its colour scheme; an explicit QT_QUICK_CONTROLS_STYLE wins. */
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("org.kdos.installer", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
