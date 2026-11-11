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
 *
 * ONE WINDOW PER SESSION. The installer is unique on the session bus as
 * io.kdos.kdos-installer-gui: a second launch (the desktop icon after the
 * Welcome tour's button) raises the first window and exits, so two
 * installers can never run pkexec against the same disk.
 */

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QWindow>

namespace
{
/*
 * THE WIZARD'S PAGE KEYS, TAKEN AHEAD OF THE SHORTCUT MAP. Kirigami's page
 * row holds its own Back and Forward shortcuts on Alt+Left and Alt+Right,
 * always enabled; a second shortcut on the same key makes both ambiguous and
 * neither fires. So the keys' ShortcutOverride is accepted here, which keeps
 * the map out of it, and the key press itself calls Main's stepBack() or
 * stepNext().
 */
class WizardKeys : public QObject
{
public:
    explicit WizardKeys(QObject *root)
        : QObject(root)
        , m_root(root)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress) {
            return QObject::eventFilter(watched, event);
        }
        const auto *k = static_cast<QKeyEvent *>(event);
        const bool alt = k->modifiers() == Qt::AltModifier;
        const bool back = (alt && k->key() == Qt::Key_Left) || k->key() == Qt::Key_Back;
        const bool next = (alt && k->key() == Qt::Key_Right) || k->key() == Qt::Key_Forward;
        if (!back && !next) {
            return QObject::eventFilter(watched, event);
        }
        if (event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true;
        }
        if (!k->isAutoRepeat()) {
            QMetaObject::invokeMethod(m_root, back ? "stepBack" : "stepNext");
        }
        return true;
    }

private:
    QObject *m_root;
};
}

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
    about.setOrganizationDomain(QByteArrayLiteral("kdos.io"));
    about.setDesktopFileName(QStringLiteral("kdos-installer"));
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("system-software-install")));
    KDBusService service(KDBusService::Unique);

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
    app.installEventFilter(new WizardKeys(engine.rootObjects().constFirst()));
    QObject::connect(&service, &KDBusService::activateRequested, &app, [&engine]() {
        if (auto *w = qobject_cast<QWindow *>(engine.rootObjects().constFirst())) {
            w->show();
            w->raise();
            w->requestActivate();
        }
    });
    return app.exec();
}
