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
 *   kdos-packs   every catalogue row and set, and the packs in the pack store
 *   medium       packs and .ktar sets on the boot medium (/mnt/iso/packs)
 *   kdos-store   Debian desktop applications no catalogue row names, from the
 *                DEP-11 catalogue in /usr/share/swcatalog; built in a box
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
    bool isOffered(const QString &id) const;
    ResultsStream *resourceForUrl(const QUrl &url);
    ResultsStream *resourceForFile(const QUrl &url);
    QList<KdosResource *> debianFor(const Filters &filter);
    KdosResource *debianResource(const AppStream::Component &component);
    Entry describeDebian(const AppStream::Component &component) const;
    void enrichFromComponent(Entry *entry, const AppStream::Component &component) const;

    StandardBackendUpdater *m_updater;
    Catalogue m_catalogue;
    PlanContext m_context;
    QHash<QString, KdosResource *> m_resources; // by KdosResource::key()
    QHash<QString, AppStream::Component> m_debianByPackage;
    QHash<QString, AppStream::Component> m_portsByPackage;
    QHash<QString, AppStream::Component> m_binhostByPackage;
    QSet<QString> m_hostIds; // component ids of the installed host ports
    std::unique_ptr<AppStream::Pool> m_pool;
    bool m_poolReady = false;
    bool m_valid = false;
    bool m_initialized = false; // the first refresh has landed
    int m_pending = 0; // refreshes in flight
    bool m_refreshAgain = false;
    QList<std::function<void()>> m_afterRefresh;
    QList<std::function<void()>> m_afterNextRefresh;
    QList<QPointer<KdosTransaction>> m_queue;
    QPointer<KdosTransaction> m_running;
    QFileSystemWatcher m_watcher;
    QTimer m_settle;
    QSet<QString> m_files; // .ktar and .kpack paths opened this session
    QSet<QString> m_chosenAtInstall; // /var/lib/kdos/apps-pending
};

}
