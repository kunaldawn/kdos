/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   fakeservices — the daemons a firewall client talks to, as fakes
 */

#include "fakeservices.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QMap>
#include <QTimer>

using Kdos::FirewallKnock;
using Kdos::FirewallKnockList;
using Kdos::FirewallService;
using Kdos::FirewallServiceList;

namespace
{
using StringMap = QMap<QString, QString>;
const QString ActivePath = QStringLiteral("/org/freedesktop/NetworkManager/ActiveConnection/1");
}

Kdos::FirewallServiceList standardServices()
{
    return {
        {QStringLiteral("ssh"), false, QStringLiteral("Logging in to this computer over SSH")},
        {QStringLiteral("http"), false, QStringLiteral("A web server on this computer")},
        {QStringLiteral("kdeconnect"), false, QStringLiteral("KDE Connect with a phone on the network")},
        {QStringLiteral("vnc"), false, QStringLiteral("Sharing this desktop over VNC (Krfb)")},
        {QStringLiteral("rdp"), false, QStringLiteral("Sharing this desktop over RDP (KRdp)")},
    };
}

/* ── io.kdos.Power1 ────────────────────────────────────────────────────── */

FakePower1::FakePower1(QObject *parent)
    : QDBusVirtualObject(parent)
{
    Kdos::registerFirewallTypes();
}

QString FakePower1::introspect(const QString &path) const
{
    Q_UNUSED(path);
    return QStringLiteral(
        "<interface name=\"io.kdos.Power1\">"
        "<method name=\"ListFirewallServices\"><arg type=\"a(sbs)\" direction=\"out\"/></method>"
        "<method name=\"SetFirewallService\"><arg type=\"s\" direction=\"in\"/><arg type=\"b\" direction=\"in\"/></method>"
        "<method name=\"ListFirewallKnocks\"><arg type=\"a(st)\" direction=\"out\"/></method>"
        "<signal name=\"FirewallServicesChanged\"/>"
        "</interface>");
}

void FakePower1::emitChanged(const QDBusConnection &connection)
{
    QDBusMessage signal = QDBusMessage::createSignal(QStringLiteral("/io/kdos/Power1"), QStringLiteral("io.kdos.Power1"), QStringLiteral("FirewallServicesChanged"));
    connection.send(signal);
}

void FakePower1::setElsewhere(const QDBusConnection &connection, const QString &name, bool open)
{
    for (FirewallService &s : services) {
        if (s.name == name) {
            s.open = open;
            if (open) {
                knocks.remove(name);
            }
        }
    }
    emitChanged(connection);
}

bool FakePower1::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    if (message.interface() != QLatin1String("io.kdos.Power1")) {
        return false;
    }
    const QString member = message.member();
    if (member == QLatin1String("ListFirewallServices")) {
        connection.send(message.createReply(QVariant::fromValue(services)));
        return true;
    }
    if (member == QLatin1String("ListFirewallKnocks")) {
        if (!knocksSupported) {
            connection.send(message.createErrorReply(QDBusError::UnknownMethod, QStringLiteral("no knocks here")));
            return true;
        }
        FirewallKnockList out;
        for (const FirewallService &s : std::as_const(services)) {
            if (!s.open) {
                out.append({s.name, knocks.value(s.name)});
            }
        }
        connection.send(message.createReply(QVariant::fromValue(out)));
        return true;
    }
    if (member == QLatin1String("SetFirewallService")) {
        const QList<QVariant> args = message.arguments();
        if (args.size() != 2 || message.signature() != QLatin1String("sb")) {
            connection.send(message.createErrorReply(QDBusError::InvalidArgs, QStringLiteral("want (sb)")));
            return true;
        }
        SetCall c;
        c.name = args.at(0).toString();
        c.open = args.at(1).toBool();
        c.interactive = message.isInteractiveAuthorizationAllowed();
        setCalls.append(c);
        if (c.name == failName) {
            connection.send(message.createErrorReply(QStringLiteral("io.kdos.Power1.Error.Failed"), failMessage));
            return true;
        }
        for (FirewallService &s : services) {
            if (s.name == c.name) {
                s.open = c.open;
                if (c.open) {
                    knocks.remove(s.name);
                }
                /* The daemon's order: the signal goes out while the change
                 * is applied, the reply after it. */
                emitChanged(connection);
                connection.send(message.createReply());
                return true;
            }
        }
        connection.send(message.createErrorReply(QStringLiteral("io.kdos.Power1.Error.NoSuchService"), QStringLiteral("no service called that")));
        return true;
    }
    return false;
}

/* ── org.freedesktop.PolicyKit1 ────────────────────────────────────────── */

FakePolkit::FakePolkit(QObject *parent)
    : QDBusVirtualObject(parent)
{
    qDBusRegisterMetaType<StringMap>();
}

QString FakePolkit::introspect(const QString &path) const
{
    Q_UNUSED(path);
    return QStringLiteral("<interface name=\"org.freedesktop.PolicyKit1.Authority\"/>");
}

bool FakePolkit::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    if (message.member() != QLatin1String("CheckAuthorization")) {
        return false;
    }
    if (message.signature() != QLatin1String("(sa{sv})sa{ss}us")) {
        connection.send(message.createErrorReply(QDBusError::InvalidArgs, message.signature()));
        return true;
    }
    const QDBusArgument subject = message.arguments().at(0).value<QDBusArgument>();
    QString kind;
    QVariantMap details;
    subject.beginStructure();
    subject >> kind >> details;
    subject.endStructure();
    subjects.append(kind + QLatin1Char(':') + details.value(QStringLiteral("name")).toString());
    actions.append(message.arguments().at(1).toString());

    QDBusArgument result;
    result.beginStructure();
    result << authorised << challenge << StringMap();
    result.endStructure();
    connection.send(message.createReply(QVariant::fromValue(result)));
    return true;
}

/* ── org.freedesktop.NetworkManager ────────────────────────────────────── */

FakeNetworkManager::FakeNetworkManager(QObject *parent)
    : QDBusVirtualObject(parent)
{
}

QString FakeNetworkManager::introspect(const QString &path) const
{
    Q_UNUSED(path);
    return QStringLiteral("<interface name=\"org.freedesktop.DBus.Properties\"/>");
}

bool FakeNetworkManager::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    if (message.interface() != QLatin1String("org.freedesktop.DBus.Properties") || message.member() != QLatin1String("Get")) {
        return false;
    }
    const QString iface = message.arguments().value(0).toString();
    const QString prop = message.arguments().value(1).toString();
    QVariant value;
    if (message.path() == QLatin1String("/org/freedesktop/NetworkManager") && prop == QLatin1String("PrimaryConnection")) {
        value = QVariant::fromValue(QDBusObjectPath(uuid.isEmpty() ? QStringLiteral("/") : ActivePath));
    } else if (message.path() == ActivePath && prop == QLatin1String("Uuid")) {
        value = uuid;
    } else if (message.path() == ActivePath && prop == QLatin1String("Id")) {
        value = id;
    } else {
        connection.send(message.createErrorReply(QDBusError::UnknownProperty, iface + QLatin1Char('.') + prop));
        return true;
    }
    connection.send(message.createReply(QVariant::fromValue(QDBusVariant(value))));
    return true;
}

/* ── org.freedesktop.Notifications ─────────────────────────────────────── */

FakeNotifications::FakeNotifications(QObject *parent)
    : QDBusVirtualObject(parent)
{
}

QString FakeNotifications::introspect(const QString &path) const
{
    Q_UNUSED(path);
    return QStringLiteral("<interface name=\"org.freedesktop.Notifications\"/>");
}

bool FakeNotifications::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    if (message.interface() != QLatin1String("org.freedesktop.Notifications")) {
        return false;
    }
    if (message.member() == QLatin1String("Notify")) {
        if (message.signature() != QLatin1String("susssasa{sv}i")) {
            connection.send(message.createErrorReply(QDBusError::InvalidArgs, message.signature()));
            return true;
        }
        calls++;
        if (failNext > 0) {
            failNext--;
            connection.send(message.createErrorReply(QDBusError::NoReply, QStringLiteral("no notification server")));
            return true;
        }
        const QList<QVariant> a = message.arguments();
        Notification n;
        n.id = m_next++;
        n.summary = a.at(3).toString();
        n.body = a.at(4).toString();
        n.actions = a.at(5).toStringList();
        n.hints = qdbus_cast<QVariantMap>(a.at(6).value<QDBusArgument>());
        n.timeout = a.at(7).toInt();
        shown.append(n);
        const QDBusMessage reply = message.createReply(QVariant::fromValue(n.id));
        if (holdMs > 0) {
            QDBusConnection c = connection;
            QTimer::singleShot(holdMs, this, [c, reply]() mutable {
                c.send(reply);
            });
        } else {
            connection.send(reply);
        }
        return true;
    }
    if (message.member() == QLatin1String("CloseNotification")) {
        closed.append(message.arguments().value(0).toUInt());
        connection.send(message.createReply());
        return true;
    }
    return false;
}

void FakeNotifications::invoke(const QDBusConnection &connection, quint32 id, const QString &key)
{
    QDBusMessage signal = QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/Notifications"),
                                                     QStringLiteral("org.freedesktop.Notifications"),
                                                     QStringLiteral("ActionInvoked"));
    signal << id << key;
    connection.send(signal);
}

void FakeNotifications::close(const QDBusConnection &connection, quint32 id, quint32 reason)
{
    QDBusMessage signal = QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/Notifications"),
                                                     QStringLiteral("org.freedesktop.Notifications"),
                                                     QStringLiteral("NotificationClosed"));
    signal << id << reason;
    connection.send(signal);
}

/* ── the bus ───────────────────────────────────────────────────────────── */

FakeBus::FakeBus()
    : connection(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("kdos-fakes")))
{
    power.services = standardServices();
}

FakeBus::~FakeBus()
{
    QDBusConnection::disconnectFromBus(QStringLiteral("kdos-fakes"));
}

bool FakeBus::servePower1()
{
    return connection.registerVirtualObject(QStringLiteral("/io/kdos/Power1"), &power) && connection.registerService(QStringLiteral("io.kdos.Power1"));
}

void FakeBus::withdrawPower1()
{
    connection.unregisterService(QStringLiteral("io.kdos.Power1"));
    connection.unregisterObject(QStringLiteral("/io/kdos/Power1"));
}

bool FakeBus::servePolkit()
{
    return connection.registerVirtualObject(QStringLiteral("/org/freedesktop/PolicyKit1/Authority"), &polkit)
        && connection.registerService(QStringLiteral("org.freedesktop.PolicyKit1"));
}

bool FakeBus::serveNetworkManager()
{
    return connection.registerVirtualObject(QStringLiteral("/org/freedesktop/NetworkManager"), &nm, QDBusConnection::SubPath)
        && connection.registerService(QStringLiteral("org.freedesktop.NetworkManager"));
}

bool FakeBus::serveNotifications()
{
    return connection.registerVirtualObject(QStringLiteral("/org/freedesktop/Notifications"), &notifications)
        && connection.registerService(QStringLiteral("org.freedesktop.Notifications"));
}
