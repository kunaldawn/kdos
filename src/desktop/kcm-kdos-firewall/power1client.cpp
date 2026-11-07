/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   power1client — the firewall half of io.kdos.Power1, asked from Qt
 */

#include "power1client.h"

#include <KLocalizedString>

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace Kdos
{

namespace
{
/* A list is the daemon reading one file; five seconds is a daemon that is
 * not there. */
constexpr int ListTimeoutMs = 5000;
/* A change waits on a person typing a password into polkit's prompt. */
constexpr int SetTimeoutMs = 5 * 60 * 1000;
}

QString Power1Client::serviceName()
{
    return QStringLiteral("io.kdos.Power1");
}

QString Power1Client::objectPath()
{
    return QStringLiteral("/io/kdos/Power1");
}

QString Power1Client::interfaceName()
{
    return QStringLiteral("io.kdos.Power1");
}

Power1Client::Power1Client(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
{
    registerFirewallTypes();
    m_bus.connect(serviceName(), objectPath(), interfaceName(), QStringLiteral("FirewallServicesChanged"), this, SLOT(onChanged()));
}

void Power1Client::onChanged()
{
    Q_EMIT changedElsewhere();
}

QString Power1Client::describe(const QDBusError &error)
{
    switch (error.type()) {
    case QDBusError::ServiceUnknown:
    case QDBusError::NoReply:
    case QDBusError::Timeout:
    case QDBusError::TimedOut:
    case QDBusError::Disconnected:
    case QDBusError::NoServer:
        return i18nc("@info", "kdos-powerd is not answering on the system bus. Start it with: service start powerd");
    case QDBusError::AccessDenied:
        return i18nc("@info", "The firewall was not changed: you may not change it, or the authentication was cancelled.");
    case QDBusError::UnknownMethod:
    case QDBusError::UnknownInterface:
    case QDBusError::UnknownObject:
        return i18nc("@info", "kdos-powerd on this system has no firewall interface.");
    default:
        break;
    }
    if (error.name() == QLatin1String("io.kdos.Power1.Error.NoSuchService")) {
        return i18nc("@info", "kdos-powerd has no service by that name.");
    }
    if (error.name() == QLatin1String("org.freedesktop.PolicyKit1.Error.NotAuthorized")
        || error.name() == QLatin1String("org.freedesktop.PolicyKit1.Error.Cancelled")) {
        return i18nc("@info", "The firewall was not changed: you may not change it, or the authentication was cancelled.");
    }
    if (!error.message().isEmpty()) {
        return error.message();
    }
    return error.name();
}

void Power1Client::listServices()
{
    const QDBusMessage call = QDBusMessage::createMethodCall(serviceName(), objectPath(), interfaceName(), QStringLiteral("ListFirewallServices"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, ListTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<FirewallServiceList> reply = *w;
        if (reply.isError()) {
            Q_EMIT listFailed(describe(reply.error()));
            return;
        }
        Q_EMIT servicesListed(reply.value());
    });
}

void Power1Client::listKnocks()
{
    const QDBusMessage call = QDBusMessage::createMethodCall(serviceName(), objectPath(), interfaceName(), QStringLiteral("ListFirewallKnocks"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, ListTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<FirewallKnockList> reply = *w;
        if (reply.isError()) {
            Q_EMIT knocksUnavailable();
            return;
        }
        QHash<QString, quint64> knocks;
        const FirewallKnockList list = reply.value();
        for (const FirewallKnock &k : list) {
            knocks.insert(k.name, k.packets);
        }
        Q_EMIT knocksListed(knocks);
    });
}

void Power1Client::setService(const QString &name, bool open)
{
    QDBusMessage call = QDBusMessage::createMethodCall(serviceName(), objectPath(), interfaceName(), QStringLiteral("SetFirewallService"));
    call << name << open;
    /* Without the flag polkit answers auth_admin_keep with a refusal rather
     * than a prompt, and the switch looks broken. */
    call.setInteractiveAuthorizationAllowed(true);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, SetTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, name, open](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            Q_EMIT setFailed(name, describe(reply.error()));
            return;
        }
        Q_EMIT serviceSet(name, open);
    });
}

} // namespace Kdos
