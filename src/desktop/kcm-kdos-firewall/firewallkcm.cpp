/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kcm_kdos_firewall — System Settings → Networking → Firewall
 *
 * Which of this computer's services answer the network, by name. The names,
 * their meaning and the ruleset are kdos-powerd's; this page lists them,
 * holds the switches until Apply, and asks the daemon over io.kdos.Power1.
 *
 * NO authActionName. System Settings would pre-authorise that action on
 * Apply through KAuth while the daemon's own check raised a second prompt
 * for the same click, and polkit's agent shows one dialog at a time. The
 * daemon's check is the one that decides, so it is the one that asks.
 */

#include <KPluginFactory>
#include <KQuickConfigModule>

#include <QDBusConnection>

#include "firewallcontroller.h"

namespace Kdos
{

class FirewallKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(Kdos::FirewallController *controller READ controller CONSTANT)

public:
    FirewallKcm(QObject *parent, const KPluginMetaData &data)
        : KQuickConfigModule(parent, data)
        , m_controller(new FirewallController(QDBusConnection::systemBus(), this))
    {
        setButtons(Help | Apply | Default);
        connect(m_controller, &FirewallController::dirtyChanged, this, &FirewallKcm::sync);
        connect(m_controller, &FirewallController::stateChanged, this, &FirewallKcm::sync);
    }

    FirewallController *controller() const
    {
        return m_controller;
    }

    void load() override
    {
        KQuickConfigModule::load();
        m_controller->load();
    }

    /* The base class would mark the page saved at once; it is saved when
     * the daemon's fresh list says so, which sync() then reports. */
    void save() override
    {
        m_controller->apply();
    }

    void defaults() override
    {
        m_controller->defaults();
    }

private:
    void sync()
    {
        setNeedsSave(m_controller->isDirty());
        setRepresentsDefaults(m_controller->representsDefaults());
    }

    FirewallController *m_controller;
};

} // namespace Kdos

K_PLUGIN_CLASS_WITH_JSON(Kdos::FirewallKcm, "kcm_kdos_firewall.json")

#include "firewallkcm.moc"
