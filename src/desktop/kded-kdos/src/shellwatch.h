/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — plasmashell brought back when it stops
 *
 * plasmashell leaves the session when the compositor goes away: it unsets
 * QT_WAYLAND_RECONNECT for itself and exits normally on a broken Wayland
 * connection, which a KWin crash or `KWin.replace` causes. KCrash restarts it
 * after a crash signal, but not after that exit, a SIGKILL or a quit, and
 * the session has no service manager to do it, because plasma_session
 * starts it once from its autostart entry.
 *
 * So this watches org.kde.plasmashell on the session bus. When the name has
 * been gone for a moment, and ksmserver answers that the session is not
 * shutting down, it starts plasmashell again, at most a few times a minute;
 * past that it says so once and leaves the desktop to the person. kded6
 * survives the compositor, so the watch outlives what it watches.
 * ---------------------------------
 */

#pragma once

#include <QDBusServiceWatcher>
#include <QList>
#include <QObject>
#include <QTimer>

namespace Kdos
{

class ShellWatch : public QObject
{
    Q_OBJECT

public:
    explicit ShellWatch(QObject *parent = nullptr);

private:
    void check();
    void respawn();
    void notifyLimit();

    QDBusServiceWatcher m_watcher;
    QTimer m_grace;
    QList<qint64> m_starts;
    bool m_asking = false;
    bool m_reported = false;
};

}
