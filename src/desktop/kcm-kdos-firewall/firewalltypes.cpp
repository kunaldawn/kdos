/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   firewalltypes — marshalling for the two firewall lists
 */

#include "firewalltypes.h"

#include <QDBusMetaType>

namespace Kdos
{

QDBusArgument &operator<<(QDBusArgument &arg, const FirewallService &service)
{
    arg.beginStructure();
    arg << service.name << service.open << service.description;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, FirewallService &service)
{
    arg.beginStructure();
    arg >> service.name >> service.open >> service.description;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const FirewallKnock &knock)
{
    arg.beginStructure();
    arg << knock.name << knock.packets;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, FirewallKnock &knock)
{
    arg.beginStructure();
    arg >> knock.name >> knock.packets;
    arg.endStructure();
    return arg;
}

void registerFirewallTypes()
{
    static const bool done = [] {
        qDBusRegisterMetaType<FirewallService>();
        qDBusRegisterMetaType<FirewallServiceList>();
        qDBusRegisterMetaType<FirewallKnock>();
        qDBusRegisterMetaType<FirewallKnockList>();
        return true;
    }();
    Q_UNUSED(done);
}

} // namespace Kdos
