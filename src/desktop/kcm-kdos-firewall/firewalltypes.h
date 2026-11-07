/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   firewalltypes — the rows io.kdos.Power1 sends, as D-Bus structs
 *
 * ListFirewallServices() answers a(sbs): name, open, description, one struct
 * per row of kdos-powerd's named-service table and in its order.
 * ListFirewallKnocks() answers a(st): a closed service's name and the packets
 * its counter has turned away since the ruleset was last loaded.
 *
 * THE FIELD ORDER IS THE CONTRACT, not the names: D-Bus structs are
 * positional, so a reordered field on either side demarshals into the wrong
 * member without an error. io.kdos.Power1.xml beside this file is the
 * interface these operators read and write.
 */

#pragma once

#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QString>

namespace Kdos
{

struct FirewallService {
    QString name;
    bool open = false;
    QString description;
};
using FirewallServiceList = QList<FirewallService>;

struct FirewallKnock {
    QString name;
    quint64 packets = 0;
};
using FirewallKnockList = QList<FirewallKnock>;

QDBusArgument &operator<<(QDBusArgument &arg, const FirewallService &service);
const QDBusArgument &operator>>(const QDBusArgument &arg, FirewallService &service);
QDBusArgument &operator<<(QDBusArgument &arg, const FirewallKnock &knock);
const QDBusArgument &operator>>(const QDBusArgument &arg, FirewallKnock &knock);

/* Once per process, before the first call that sends or reads either list. */
void registerFirewallTypes();

} // namespace Kdos

Q_DECLARE_METATYPE(Kdos::FirewallService)
Q_DECLARE_METATYPE(Kdos::FirewallServiceList)
Q_DECLARE_METATYPE(Kdos::FirewallKnock)
Q_DECLARE_METATYPE(Kdos::FirewallKnockList)
