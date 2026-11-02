/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   fakeservices — io.kdos.Power1, polkit, NetworkManager and a notification
 *                  server, served on a private session bus
 *
 * The fakes own the real bus names on a connection of their own, so a client
 * on another connection — the controller under test, or kdos-fwask in a child
 * process pointed at this bus as its system bus — reaches them through the
 * daemon exactly as it reaches the real ones, marshalling included.
 */

#pragma once

#include <QDBusConnection>
#include <QDBusVirtualObject>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "firewalltypes.h"

struct SetCall {
    QString name;
    bool open = false;
    bool interactive = false;
};

class FakePower1 : public QDBusVirtualObject
{
    Q_OBJECT

public:
    explicit FakePower1(QObject *parent = nullptr);

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;

    /* FirewallServicesChanged, as kdos-powerd emits it. */
    void emitChanged(const QDBusConnection &connection);
    /* Change a row as `kdos-power firewall` would, signal included. */
    void setElsewhere(const QDBusConnection &connection, const QString &name, bool open);

    Kdos::FirewallServiceList services;
    QHash<QString, quint64> knocks;
    bool knocksSupported = true;
    QString failName;
    QString failMessage = QStringLiteral("the ruleset would not load; nothing changed");
    QList<SetCall> setCalls;
};

class FakePolkit : public QDBusVirtualObject
{
    Q_OBJECT

public:
    explicit FakePolkit(QObject *parent = nullptr);

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;

    bool authorised = false;
    bool challenge = true;
    QStringList subjects;
    QStringList actions;
};

class FakeNetworkManager : public QDBusVirtualObject
{
    Q_OBJECT

public:
    explicit FakeNetworkManager(QObject *parent = nullptr);

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;

    /* Empty is "no primary connection". */
    QString uuid = QStringLiteral("3f1c2d0e-9a8b-4c7d-8e6f-5a4b3c2d1e0f");
    QString id = QStringLiteral("Home <5G>");
};

struct Notification {
    quint32 id = 0;
    QString summary;
    QString body;
    QStringList actions;
    QVariantMap hints;
    qint32 timeout = -1;
};

class FakeNotifications : public QDBusVirtualObject
{
    Q_OBJECT

public:
    explicit FakeNotifications(QObject *parent = nullptr);

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;

    void invoke(const QDBusConnection &connection, quint32 id, const QString &key);
    void close(const QDBusConnection &connection, quint32 id, quint32 reason);

    QList<Notification> shown;
    QList<quint32> closed;

private:
    quint32 m_next = 1;
};

/* The connection the fakes live on, and the names it owns. */
class FakeBus
{
public:
    FakeBus();
    ~FakeBus();

    bool servePower1();
    void withdrawPower1();
    bool servePolkit();
    bool serveNetworkManager();
    bool serveNotifications();

    QDBusConnection connection;
    FakePower1 power;
    FakePolkit polkit;
    FakeNetworkManager nm;
    FakeNotifications notifications;
};

/* The rows the real table starts with, closed. */
Kdos::FirewallServiceList standardServices();
