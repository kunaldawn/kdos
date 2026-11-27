/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — KdosBackend: packs, boxes, the store lane and host ports
 *
 * WHAT IT SHOWS, BY ORIGIN:
 *
 *   kdos-packs   the packs in the pack store, and each catalogue row whose
 *                pack is there
 *   medium       packs and .ktar sets on the boot medium (/mnt/iso/packs),
 *                and each catalogue row whose pack is only there
 *   kdos-store   catalogue rows with no pack anywhere, every set, and Debian
 *                desktop applications no catalogue row names, from the DEP-11
 *                catalogue in /usr/share/swcatalog less the packages
 *                /usr/share/kdos/discover/debian-exclude names; each is
 *                built in a box
 *   kdos-boxes   boxes that no application above accounts for
 *   kdos-ports   host software: the kdos-ports AppStream catalogue of what
 *                is installed, with kpkg's versions, and the kdos-binhost
 *                catalogue of what the release's signed binary host holds,
 *                which `kdos update fetch` keeps in /var/lib/kdos/binhost
 *   local        a .ktar or .kpack opened from a file manager
 *
 * NOTHING HERE DOWNLOADS. A refresh asks the local tools and reads local
 * files, and "Check for updates" is the same refresh: the only network a
 * transaction reaches is the store lane's apt, or the release binhost a host
 * port is installed from, each of which the user asked for by
 * pressing Install on a resource that says it needs it.
 *
 * THE DEBIAN LANE IS LAZY. The DEP-11 catalogue holds thousands of
 * applications, so a resource is made for one only when a search or a
 * category asks for it, or when its box exists; the rest stay rows in the
 * AppStream pool.
 * ---------------------------------
 */

#pragma once

#include "KdosEntry.h"
#include "KdosParse.h"
#include "KdosPlan.h"

#include <resources/AbstractResourcesBackend.h>
#include <Transaction/Transaction.h>

#include <AppStreamQt/component.h>

#include <QFileSystemWatcher>
#include <QHash>
#include <QPointer>
#include <QTimer>

#include <functional>
#include <memory>

class StandardBackendUpdater;

namespace AppStream
{
class Pool;
}

namespace Kdos
{

class KdosResource;
class KdosTransaction;

// A launcher kdos-appbox genlaunchers wrote: its file and its Icon= value.
struct BoxLauncher {
    QString path;
    QString icon;
};

// The user's launchers that run in a box, by that box: `Exec=kdos-appbox -b
// <box> run ...` in the applications directory, marked X-KDOS-Alien and
// shown. A box with several, a suite's pack, is filed under the first by
// file name.
QHash<QString, BoxLauncher> boxLaunchers();

// One host desktop application as a Debian component of the same name is
// compared with it (hostTwinByName): its package, the package's stem (its
// leading run of ASCII letters, case-folded) and its homepage as
// homepageHost gives it.
struct HostFace {
    QString package;
    QString stem;
    QString homepage;
};

class KdosBackend : public AbstractResourcesBackend
{
    Q_OBJECT
public:
    explicit KdosBackend(QObject *parent = nullptr);
    ~KdosBackend() override;

    bool isValid() const override;
    ResultsStream *search(const AbstractResourcesBackend::Filters &filter) override;
    AbstractReviewsBackend *reviewsBackend() const override;
    AbstractBackendUpdater *backendUpdater() const override;
    int updatesCount() const override;
    QString displayName() const override;
    bool hasApplications() const override;
    int fetchingUpdatesProgress() const override;
    QList<std::shared_ptr<Category>> category() const override;
    InlineMessage *explainDysfunction() const override;

    // What the plans are made against, as of the last refresh.
    PlanContext planContext() const;
    // Display name for an origin string.
    static QString originName(const QString &origin);

    // Called by a transaction when its last step ended, or it failed or was
    // cancelled. The machine is read again, then the status is set.
    void transactionFinished(KdosTransaction *transaction, Transaction::Status status);

    // Runs `kdos-box profile <box>` for the resource's box and hands it the
    // answer.
    void readBoxGrants(KdosResource *resource);

public Q_SLOTS:
    Transaction *installApplication(AbstractResource *app, const AddonList &addons) override;
    Transaction *installApplication(AbstractResource *app) override;
    Transaction *removeApplication(AbstractResource *app) override;
    void checkForUpdates() override;

private:
    struct Snapshot;

    void refresh(std::function<void()> then = {});
    void collect(Snapshot *snap);
    void apply(const Snapshot &snap);
    void loadPool();
    void readHostCatalogues();
    void enqueue(KdosTransaction *transaction);
    void startNext();

    KdosResource *upsert(const Entry &entry, QSet<QString> *seen);
    // The twin of a port that a list leaves out: the one `install =` does
    // not prefer (preferSource: it says `source`), and the one built here
    // once the port is installed.
    bool isSecondTwin(const KdosResource *res, bool preferSource) const;
    bool isOffered(const QString &id) const;
    // A box exists: one in the last refresh, or before the first a profile
    // under ~/.config/kdos/boxes.
    bool hasBoxes() const;
    ResultsStream *resourceForUrl(const QUrl &url);
    QVector<StreamResult> resourcesForUrl(const QUrl &url);
    ResultsStream *resourceForFile(const QUrl &url);
    QList<KdosResource *> debianFor(const Filters &filter);
    KdosResource *debianResource(const AppStream::Component &component);
    Entry describeDebian(const AppStream::Component &component) const;
    void enrichFromComponent(Entry *entry, const AppStream::Component &component) const;
    // Gives a resource with a box the launcher genlaunchers wrote for it,
    // and that launcher's icon in place of a stand-in of its category.
    void applyLauncher(Entry *entry, bool iconIsStandIn) const;
    // The catalogue row a Debian component is: the row that packs its
    // package, or one of the same name of whose packages one is the same
    // program (hostTwinByName over m_catalogueNames). Empty for none.
    QString catalogueRowOf(const AppStream::Component &component) const;

    StandardBackendUpdater *m_updater;
    Catalogue m_catalogue;
    PlanContext m_context;
    QHash<QString, KdosResource *> m_resources; // by KdosResource::key()
    QHash<QString, AppStream::Component> m_debianByPackage;
    QHash<QString, AppStream::Component> m_portsByPackage;
    QHash<QString, AppStream::Component> m_binhostByPackage;
    QSet<QString> m_hostIds; // every component id and desktop-id launchable of the kdos-ports and kdos-binhost catalogues, without a trailing .desktop
    QHash<QString, QList<HostFace>> m_hostNames; // the desktop applications of the same catalogues, by case-folded name
    std::unique_ptr<AppStream::Pool> m_pool;
    bool m_poolReady = false;
    bool m_valid = false;
    bool m_initialized = false; // the first refresh has landed
    int m_pending = 0; // refreshes in flight
    bool m_refreshAgain = false;
    QList<std::function<void()>> m_afterRefresh;
    QList<std::function<void()>> m_afterNextRefresh;
    QList<std::function<void()>> m_afterPool; // run by the first refresh to land with the pool loaded
    QList<QPointer<KdosTransaction>> m_queue;
    QPointer<KdosTransaction> m_running;
    QFileSystemWatcher m_watcher;
    QTimer m_settle;
    QSet<QString> m_files; // .ktar and .kpack paths opened this session
    QSet<QString> m_chosenAtInstall; // /var/lib/kdos/apps-pending
    QSet<QString> m_debianExclude; // Debian packages the Debian lane never offers
    QHash<QString, BoxLauncher> m_launchers; // the user's box launchers, by box, as of the last refresh
    QHash<QString, QList<HostFace>> m_catalogueNames; // the catalogue's app rows by case-folded name, a face per package
    QHash<QString, QString> m_catalogueByPackage; // the catalogue row that packs each package, by package
    mutable bool m_categoriesRead = false; // category() has answered once
    mutable bool m_listedBoxes = false; // and its answer had the Boxes page
};

}
