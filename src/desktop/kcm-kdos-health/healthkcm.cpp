/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kcm_kdos_health — Info Center and System Settings → Health
 *
 * `kdos doctor`, a row per check, beside About this System: what passed,
 * what could not be checked on this machine, and what needs attention, the
 * last in the negative colour. It changes nothing, so it has no Apply.
 */

#include <KPluginFactory>
#include <KQuickConfigModule>

#include "healthcontroller.h"

namespace Kdos
{

class HealthKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(Kdos::HealthController *controller READ controller CONSTANT)

public:
    HealthKcm(QObject *parent, const KPluginMetaData &data)
        : KQuickConfigModule(parent, data)
        , m_controller(new HealthController(QString(), this))
    {
        setButtons(Help);
    }

    HealthController *controller() const
    {
        return m_controller;
    }

    void load() override
    {
        KQuickConfigModule::load();
        m_controller->refresh();
    }

private:
    HealthController *m_controller;
};

} // namespace Kdos

K_PLUGIN_CLASS_WITH_JSON(Kdos::HealthKcm, "kcm_kdos_health.json")

#include "healthkcm.moc"
