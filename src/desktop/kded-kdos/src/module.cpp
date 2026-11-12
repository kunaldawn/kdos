/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — the kded6 module
 * ---------------------------------
 */

#include "module.h"
#include "accentsync.h"
#include "sharestart.h"
#include "shellwatch.h"
#include "stickoffer.h"

#include <KPluginFactory>

namespace Kdos
{

Module::Module(QObject *parent, const QVariantList &)
    : KDEDModule(parent)
    , m_sync(new AccentSync(this))
    , m_stick(new StickOffer(this))
    , m_share(new ShareStart(this))
    , m_shell(new ShellWatch(this))
{
    connect(m_sync, &AccentSync::synced, this, &Module::accentSynced);
    connect(m_sync, &AccentSync::failed, this, &Module::accentSyncFailed);
}

QString Module::accent() const
{
    return m_sync->accent();
}

QString Module::syncState() const
{
    return m_sync->state();
}

QString Module::lastError() const
{
    return m_sync->lastError();
}

void Module::resync()
{
    m_sync->resync();
}

}

K_PLUGIN_FACTORY_WITH_JSON(KdosModuleFactory, "kdos.json", registerPlugin<Kdos::Module>();)

#include "module.moc"
