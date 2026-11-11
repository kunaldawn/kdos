/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — the kded6 module
 *
 * Four jobs in one module, because each is something the session has to
 * notice while nobody is looking at a KDOS window: the accent a Plasma page
 * chose, a stick carrying an application set, a login that can share
 * folders from Dolphin, for which Samba is started, and a plasmashell that
 * has stopped, which is started again.
 *
 * Its state is on the session bus at org.kde.kded6 /modules/kdos, interface
 * io.kdos.Kded1, for a settings page or a script to ask: the accent the
 * system was last brought to, whether a run is going or failed, and why.
 * The module id is a bare word because kded registers each module at
 * /modules/<id>, and a dotted id is not a valid object path.
 * ---------------------------------
 */

#pragma once

#include <KDEDModule>

#include <QVariantList>

namespace Kdos
{

class AccentSync;
class ShareStart;
class ShellWatch;
class StickOffer;

class Module : public KDEDModule
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.kdos.Kded1")

public:
    Module(QObject *parent, const QVariantList &args);

public Q_SLOTS:
    Q_SCRIPTABLE QString accent() const;
    Q_SCRIPTABLE QString syncState() const;
    Q_SCRIPTABLE QString lastError() const;
    Q_SCRIPTABLE void resync();

Q_SIGNALS:
    Q_SCRIPTABLE void accentSynced(const QString &accent);
    Q_SCRIPTABLE void accentSyncFailed(const QString &accent, const QString &message);

private:
    AccentSync *m_sync;
    StickOffer *m_stick;
    ShareStart *m_share;
    ShellWatch *m_shell;
};

}
