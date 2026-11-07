/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   power1client — the firewall half of io.kdos.Power1, asked from Qt
 *
 * EVERY CALL IS ASYNCHRONOUS. A blocking call from the settings window's
 * thread freezes the window for as long as the daemon takes, and
 * SetFirewallService waits on a polkit prompt a person may leave open for
 * minutes; its timeout is sized for that prompt, not for the daemon.
 *
 * THE BUS IS A PARAMETER. The page talks to the system bus; the tests serve a
 * fake io.kdos.Power1 on a private session bus and hand that connection in,
 * so the code under test is the code that ships.
 *
 * An error reaches the caller as a sentence a person can act on. The
 * daemon's own refusal ("the ruleset would not load; nothing changed") is
 * passed through as it is, because it already is one.
 */

#pragma once

#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QString>

#include "firewalltypes.h"

class QDBusError;

namespace Kdos
{

class Power1Client : public QObject
{
    Q_OBJECT

public:
    static QString serviceName();
    static QString objectPath();
    static QString interfaceName();

    explicit Power1Client(const QDBusConnection &bus, QObject *parent = nullptr);

    void listServices();
    void listKnocks();
    void setService(const QString &name, bool open);

    /* A D-Bus error as the sentence the page shows. */
    static QString describe(const QDBusError &error);

Q_SIGNALS:
    void servicesListed(const Kdos::FirewallServiceList &services);
    void listFailed(const QString &message);
    void knocksListed(const QHash<QString, quint64> &knocks);
    /* A daemon without the knock counters answers UnknownMethod; the page
     * then shows no counts rather than an error. */
    void knocksUnavailable();
    void serviceSet(const QString &name, bool open);
    void setFailed(const QString &name, const QString &message);
    /* FirewallServicesChanged, which the daemon emits after every change
     * it applied, whichever face asked for it — this page's own calls
     * included. Only the connection that owns io.kdos.Power1 is heard:
     * QtDBus follows the name's owner for a connection made by name. */
    void changedElsewhere();

private Q_SLOTS:
    void onChanged();

private:
    QDBusConnection m_bus;
};

} // namespace Kdos
