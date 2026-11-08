/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — Samba started for the first shared folder
 *
 * smbd does not run at boot until something is shared, and a folder shared
 * from Dolphin's Share tab is a file `net usershare` writes into
 * /var/lib/samba/usershares while smbd is still down. This watches that
 * directory and, when it goes from holding no share to holding one, calls
 * io.kdos.Power1.StartFileSharing on the system bus: kdos-powerd asks polkit
 * (io.kdos.filesharing.start) and runs Samba's service script, which serves
 * the share from then on.
 *
 * The directory is root's, group sambashare, mode 1770, so only a session of
 * a member can watch it, and only a member can share a folder at all. In any
 * other session there is nothing to watch, and the module says so once in its
 * log. The call names nothing: what starts is fixed on the daemon's side.
 * ---------------------------------
 */

#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QTimer>

namespace Kdos
{

class ShareWatch : public QObject
{
    Q_OBJECT

public:
    explicit ShareWatch(QObject *parent = nullptr);

private:
    void evaluate();
    void start();
    void notifyFailure(const QString &message);

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    /* Whether the last look found a share: the call is made on the change
     * from none to some, not on every write a share makes. */
    bool m_shared = false;
    bool m_asking = false;
    QSet<QString> m_reported;
};

}
