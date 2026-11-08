/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — the offer to import a set found on a stick
 *
 * When a removable volume becomes accessible — mounted from the Device
 * Notifier, by a file manager, or automatically — the top of it is looked at
 * once for `*.ktar` files, the application sets `kdos app export` writes. If
 * there are any, one notification offers to open each in Discover, whose
 * KDOS backend imports a set offline and verifies every pack as it goes.
 *
 * Nothing is imported here and nothing is read past the directory listing:
 * the offer is the whole of it, and the person decides. A volume already
 * mounted when the session starts is not offered, so a stick left in is not
 * announced at every login; it is offered the next time it is mounted.
 * ---------------------------------
 */

#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

class KNotification;

namespace Solid
{
class Device;
}

namespace Kdos
{

class StickOffer : public QObject
{
    Q_OBJECT

public:
    explicit StickOffer(QObject *parent = nullptr);

private:
    void watch(const Solid::Device &device);
    void deviceAdded(const QString &udi);
    void deviceRemoved(const QString &udi);
    void scan(const QString &udi);
    void offer(const QString &udi, const QString &mountPoint, const QString &label,
               const QStringList &sets);
    void withdraw(const QString &udi);

    QSet<QString> m_watched;
    QHash<QString, QPointer<KNotification>> m_shown;
};

}
