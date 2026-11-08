/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — the offer to import a set found on a stick
 * ---------------------------------
 */

#include "stickoffer.h"
#include "debug.h"
#include "logic.h"

#include <KLocalizedString>
#include <KNotification>

#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <QFileInfo>
#include <QFutureWatcher>
#include <QProcess>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

namespace Kdos
{

/* Sets offered from one volume, each its own action button. A stick holding
 * more is a stick somebody is organising, and Discover opens any of them. */
static constexpr int MAX_SETS = 3;

/* Directory entries looked at, at most, before the scan gives up. */
static constexpr int SCAN_LIMIT = 4096;

static bool removable(const Solid::Device &device)
{
    Solid::Device d = device;

    while (d.isValid() && !d.is<Solid::StorageDrive>()) {
        d = d.parent();
    }
    if (!d.isValid()) {
        return false;
    }
    const auto *drive = d.as<Solid::StorageDrive>();

    return drive->isRemovable() || drive->isHotpluggable();
}

static QString labelOf(const Solid::Device &device, const QString &mountPoint)
{
    if (const auto *volume = device.as<Solid::StorageVolume>(); volume && !volume->label().isEmpty()) {
        return volume->label();
    }
    if (!device.description().isEmpty()) {
        return device.description();
    }
    return QFileInfo(mountPoint).fileName();
}

StickOffer::StickOffer(QObject *parent)
    : QObject(parent)
{
    auto *notifier = Solid::DeviceNotifier::instance();

    connect(notifier, &Solid::DeviceNotifier::deviceAdded, this, &StickOffer::deviceAdded);
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, this, &StickOffer::deviceRemoved);

    const auto present = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);

    for (const Solid::Device &device : present) {
        watch(device);
    }
}

void StickOffer::watch(const Solid::Device &device)
{
    const QString udi = device.udi();

    if (m_watched.contains(udi) || !device.is<Solid::StorageAccess>() || !removable(device)) {
        return;
    }
    m_watched.insert(udi);

    const auto *access = device.as<Solid::StorageAccess>();

    connect(access, &Solid::StorageAccess::accessibilityChanged, this,
            [this](bool accessible, const QString &changed) {
                if (accessible) {
                    scan(changed);
                } else {
                    withdraw(changed);
                }
            });
}

void StickOffer::deviceAdded(const QString &udi)
{
    const Solid::Device device(udi);

    watch(device);
    if (m_watched.contains(udi) && device.as<Solid::StorageAccess>()->isAccessible()) {
        scan(udi);
    }
}

void StickOffer::deviceRemoved(const QString &udi)
{
    withdraw(udi);
    m_watched.remove(udi);
}

void StickOffer::scan(const QString &udi)
{
    const Solid::Device device(udi);
    const auto *access = device.as<Solid::StorageAccess>();

    if (!access || !access->isAccessible() || access->filePath().isEmpty()) {
        return;
    }
    const QString mountPoint = access->filePath();
    const QString label = labelOf(device, mountPoint);

    /* Off kded's own thread: kded serves every module from one event loop,
     * and a slow or failing stick must not stall the others while its root
     * is listed. */
    auto *watcher = new QFutureWatcher<QStringList>(this);

    connect(watcher, &QFutureWatcher<QStringList>::finished, this,
            [this, watcher, udi, mountPoint, label] {
                const QStringList sets = watcher->result();

                watcher->deleteLater();

                const Solid::Device now(udi);
                const auto *still = now.as<Solid::StorageAccess>();

                /* Unmounted, or mounted somewhere else, while the listing
                 * ran: the paths found are no longer the stick's. */
                if (sets.isEmpty() || !still || !still->isAccessible() || still->filePath() != mountPoint) {
                    return;
                }
                offer(udi, mountPoint, label, sets);
            });
    watcher->setFuture(QtConcurrent::run([mountPoint] {
        return ktarSets(mountPoint, MAX_SETS, SCAN_LIMIT);
    }));
}

void StickOffer::offer(const QString &udi, const QString &mountPoint, const QString &label,
                       const QStringList &sets)
{
    withdraw(udi);

    const QString discover = QStandardPaths::findExecutable(QStringLiteral("plasma-discover"));
    QStringList names;

    for (const QString &set : sets) {
        names.append(QFileInfo(set).fileName().toHtmlEscaped());
    }

    auto *n = new KNotification(QStringLiteral("importOffer"));

    n->setComponentName(QStringLiteral("kded_kdos"));
    n->setIconName(QStringLiteral("drive-removable-media"));
    /* The title is drawn as plain text, so the label goes in as it is; an
     * escaped one would show its `&amp;`. */
    n->setTitle(i18nc("@title %1 is a volume label", "Applications on %1", label));

    /* Every name in the text comes off the stick, so it is escaped: the text
     * is rich text, and a file called `<a href=…>` would otherwise be a
     * link. */
    if (discover.isEmpty()) {
        n->setText(i18ncp("@info",
                          "%2 holds an exported application set. Import it with "
                          "<b>kdos app import</b> in a terminal.",
                          "%2 holds %1 exported application sets. Import them with "
                          "<b>kdos app import</b> in a terminal.",
                          sets.size(), label.toHtmlEscaped()));
    } else {
        n->setText(i18ncp("@info %2 is a volume label, %3 file names",
                          "%2 holds the exported application set %3.",
                          "%2 holds %1 exported application sets: %3.",
                          sets.size(), label.toHtmlEscaped(),
                          names.join(QStringLiteral(", "))));

        for (const QString &set : sets) {
            const QString text = sets.size() == 1
                ? i18nc("@action:button", "Open in Discover")
                : i18nc("@action:button %1 is a file name", "Open %1",
                        QFileInfo(set).fileName());
            KNotificationAction *action = n->addAction(text);

            /* Argument vector only: the path is a name from the stick, and
             * a shell between it and Discover would run whatever it says. */
            connect(action, &KNotificationAction::activated, this, [discover, set] {
                if (!QProcess::startDetached(discover, {QStringLiteral("--local-filename"), set})) {
                    qCWarning(KDOS_KDED) << "could not start" << discover << "on" << set;
                }
            });
        }
    }

    m_shown.insert(udi, n);
    qCInfo(KDOS_KDED) << "offering" << sets.size() << "application set(s) from" << mountPoint;
    n->sendEvent();
}

void StickOffer::withdraw(const QString &udi)
{
    if (QPointer<KNotification> n = m_shown.take(udi); n) {
        n->close();
    }
}

}
