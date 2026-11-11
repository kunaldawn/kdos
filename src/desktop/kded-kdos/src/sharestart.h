/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — Samba started for a session that can share folders
 *
 * smbd must be running before the first folder can be shared: `net usershare
 * add` turns every name in a share's ACL, Dolphin's `Everyone` among them,
 * into a SID by asking the local smbd, and with smbd down it fails and writes
 * no share. Samba's service script starts smbd at boot once the sambashare
 * group has a member; an account the Share tab has just added to the group
 * is a member from its next login, which can be before the next boot. So at
 * login, in a session of a member, this calls
 * io.kdos.Power1.StartFileSharing on the system bus: kdos-powerd asks polkit
 * (io.kdos.filesharing.start) and runs the service script, whose own test
 * decides again, and which starts nothing when smbd already runs.
 *
 * Membership is read from what the session's credentials can do: the
 * usershare directory is root's, group sambashare, mode 1770, so only a
 * member can write into it, and only a member can share a folder at all. In
 * any other session nothing is called, and the module says so once in its
 * log. The call names nothing: what starts is fixed on the daemon's side.
 * ---------------------------------
 */

#pragma once

#include <QObject>
#include <QSet>

namespace Kdos
{

class ShareStart : public QObject
{
    Q_OBJECT

public:
    explicit ShareStart(QObject *parent = nullptr);

private:
    void start();
    void notifyFailure(const QString &message);

    QSet<QString> m_reported;
};

}
