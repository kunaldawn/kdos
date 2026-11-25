/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — KdosNotifier: the A/B update in Discover's tray
 *
 * Discover's tray loads only discover-notifier/ plugins, and its sysupdate1
 * backend ships none, so without this one the tray never learns of a system
 * update. Three answers make the tray's state:
 *
 *   hasUpdates          io.kdos.Slots1's State `offer`, the version CheckNew
 *                       would answer; /var/lib/kdos/update.json's `offer`
 *                       when kdos-sysupdated does not answer
 *   hasSecurityUpdates  an offer exists and closes at least one finding of
 *                       `kdos cve --json`: the finding's port is among the
 *                       update's packages, at or past its fixed version
 *   needsReboot         a staged update (KdosParse's stagedUpdate): State
 *                       `rebootRequired`, a slot armed by `kdos-bootctl try`
 *                       and not yet booted, holding a version newer than the
 *                       running one by ListSlots; `kdos-bootctl status
 *                       --json` when the daemon does not answer
 *
 * IT READS, IT NEVER FETCHES. CheckNew refreshes the signed index when the
 * last fetch is an hour old, so a tray that called it would reach the network
 * at every login. State's `offer` is the same answer worked out from the
 * update document already on disk, which the nightly offline check and the
 * person's own "Check for updates" leave behind.
 *
 * AN UPDATE BEING ACQUIRED STAYS VISIBLE, ONCE. A job State reports that
 * began while org.kde.discover was not on the session bus gets the plugin's
 * own job in the notification area, "Preparing the system update", N of M;
 * one that began while Discover ran is taken for Discover's, reported by its
 * transaction. Decided once per job: a second entry would leave two for one
 * update.
 *
 * THE TRIAL IS NOT THE TRAY'S. BackendNotifierModule has updates, security
 * updates and a pending restart, and no way to say "running 0.2.14 on
 * trial"; kdos-notice posts that notice from the same State. A slot armed
 * at the running version or an older one is such a switch, not an update:
 * Discover would answer it with "Restart is required … for the updates to
 * take effect", so it is no pending restart here.
 * ---------------------------------
 */

#pragma once

#include "KdosParse.h"

#include <BackendNotifierModule.h>

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class QTimer;

namespace Kdos
{

class UpdateJob;

class KdosNotifier : public BackendNotifierModule
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DISCOVER_NOTIFIER_IID)
    Q_INTERFACES(BackendNotifierModule)
public:
    explicit KdosNotifier(QObject *parent = nullptr);
    ~KdosNotifier() override;

    void recheckSystemUpdateNeeded() override;
    bool hasUpdates() override;
    bool hasSecurityUpdates() override;
    bool needsReboot() const override;

private Q_SLOTS:
    void slotsPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    void readUpdateDocument();
    void readSecurity();
    void readSlots();
    void readBootctl();
    void readSlotList();
    void applySlots(const SlotsState &state, bool fromDaemon);
    void recompute();
    void trackJob(const SlotsState &state);
    void runTool(const QString &program, const QStringList &arguments, std::function<void(const QByteArray &, bool)> done);

    QFileSystemWatcher m_watcher;
    QTimer *m_settle;
    QByteArray m_updateDocument;
    QByteArray m_cveDocument;
    SlotsState m_slots;
    bool m_hasUpdates = false;
    bool m_hasSecurity = false;
    bool m_needsReboot = false;
    QPointer<UpdateJob> m_job;
};

}
