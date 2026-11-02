/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kcm_kdos_look — System Settings → Appearance & Style → KDOS Look
 *
 * The accent everywhere in one click, whether the places that wear it agree,
 * and Phosphor mode. The accents themselves are colour schemes and global
 * themes on the Colors and Global Theme pages; what this page adds is the
 * one action that also repaints the terminals, the lock and login screens,
 * the splash and the boot menu, and the switch for the CRT effect.
 *
 * EVERY CONTROL ACTS AT ONCE, so the page has no Apply: an accent change is a
 * command that repaints the machine, and holding it until Apply would show a
 * swatch picked while nothing moved.
 */

#include <KPluginFactory>
#include <KQuickConfigModule>

#include "lookcontroller.h"

namespace Kdos
{

class LookKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(Kdos::LookController *controller READ controller CONSTANT)

public:
    LookKcm(QObject *parent, const KPluginMetaData &data)
        : KQuickConfigModule(parent, data)
        , m_controller(new LookController(LookController::Paths{}, this))
    {
        setButtons(Help);
    }

    LookController *controller() const
    {
        return m_controller;
    }

    void load() override
    {
        KQuickConfigModule::load();
        m_controller->refresh();
    }

private:
    LookController *m_controller;
};

} // namespace Kdos

K_PLUGIN_CLASS_WITH_JSON(Kdos::LookKcm, "kcm_kdos_look.json")

#include "lookkcm.moc"
