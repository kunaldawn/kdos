/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — KdosBackend: packs, boxes, the store lane and host ports
 * ---------------------------------
 */

#include "KdosBackend.h"
#include "KdosPackd.h"
#include "KdosResource.h"
#include "KdosTransaction.h"
#include "libdiscover_kdos_debug.h"

#include <Category/Category.h>
#include <Category/CategoryModel.h>
#include <appstream/AppStreamUtils.h>
#include <resources/StandardBackendUpdater.h>

#include <AppStreamQt/developer.h>
#include <AppStreamQt/metadata.h>
#include <AppStreamQt/pool.h>

#include <KLocalizedString>
#include <KTar>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QXmlStreamReader>

#include <algorithm>
#include <memory>

using namespace Qt::StringLiterals;

namespace Kdos
{

// Where the shipped data lives; each has the override its own tool reads.
static QString cataloguePath()
{
    const QString env = qEnvironmentVariable("KDOS_CATALOGUE");
    return env.isEmpty() ? u"/usr/share/kdos/appstore/catalogue"_s : env;
}

static QString mediumPacksDir()
{
    const QString env = qEnvironmentVariable("KDOS_PACK_MEDIUM");
    return env.isEmpty() ? u"/mnt/iso/packs"_s : env;
}

static QString packStoreDir()
{
    const QString env = qEnvironmentVariable("KDOS_PACK_STORE");
    return env.isEmpty() ? u"/var/lib/kdos/packs"_s : env;
}

// What the installer recorded when it could neither import nor build: group
// and catalogue ids, one a line. The first login's offer opens Discover on
// the "Selected during install" page these make.
static QString pendingPath()
{
    const QString env = qEnvironmentVariable("KDOS_APPS_PENDING");
    return env.isEmpty() ? u"/var/lib/kdos/apps-pending"_s : env;
}

// The release's signed binary host, which `kdos update fetch` keeps and
// `kdos update install` installs from. It holds its AppStream catalogue as a
// catalogue root, swcatalog/xml/kdos-binhost.xml, origin kdos-binhost: the
// ports an install can add to this slot. With no catalogue there, the ports
// lane offers nothing to install.
static QString binhostCatalogue()
{
    const QString env = qEnvironmentVariable("KDOS_BINHOST_CACHE");
    return (env.isEmpty() ? u"/var/lib/kdos/binhost"_s : env) + u"/swcatalog/xml/kdos-binhost.xml"_s;
}

// The route `kdos update install` takes when nothing names one: `install =`,
// which System Settings › System Versions writes. Read at every search, so a
// change there is what the next list shows.
static bool preferSource()
{
    const QString env = qEnvironmentVariable("KDOS_UPDATE_CONF");
    QFile f(env.isEmpty() ? u"/etc/kdos/update.conf"_s : env);
    return f.open(QIODevice::ReadOnly) && installsFromSource(f.readAll());
}

// Debian applications the Debian lane never offers: one package name a
// line, # for a comment. Each is one that cannot do its job from a box on
// KWin's Wayland session, such as a magnifier or a pointer tool that works
// only on an X server's own screen.
static QString debianExcludePath()
{
    const QString env = qEnvironmentVariable("KDOS_DEBIAN_EXCLUDE");
    return env.isEmpty() ? u"/usr/share/kdos/discover/debian-exclude"_s : env;
}

static QSet<QString> readDebianExclude()
{
    QSet<QString> out;
    QFile f(debianExcludePath());
    if (!f.open(QIODevice::ReadOnly))
        return out;
    for (const QByteArray &raw : f.readAll().split('\n')) {
        const QString line = QString::fromUtf8(raw.left(raw.indexOf('#') < 0 ? raw.size() : raw.indexOf('#'))).trimmed();
        if (!line.isEmpty())
            out.insert(line);
    }
    return out;
}

// The catalogue roots: /usr/share/swcatalog, which holds Debian's DEP-11
// catalogue and the image's kdos-ports one, or KDOS_SWCATALOG's
// colon-separated list in its place for a test.
static QStringList swcatalogRoots()
{
    const QString only = qEnvironmentVariable("KDOS_SWCATALOG");
    if (!only.isEmpty())
        return only.split(u':', Qt::SkipEmptyParts);
    return {u"/usr/share/swcatalog"_s};
}

// A process that has not answered in this long is treated as having said
// nothing; `kdos-appbox catalogue` asks podman, which can hang on a wedged
// storage lock.
static constexpr int ToolTimeoutMs = 60000;

// The Debian lane answers at most this many applications per query: a
// category of the whole archive is thousands, and search narrows it.
static constexpr int DebianQueryLimit = 300;

struct KdosBackend::Snapshot {
    QList<CatalogueListing> listing;
    bool listingOk = false;
    QList<CatalogueGroup> groups;
    QHash<QString, ChainRow> chain; // base and runtime rows, from --chain
    QList<BoxRow> boxes;
    QList<PackRow> packs;
    QString packdError;
    QHash<QString, Meta> packMeta;
    QHash<QString, QString> packIcons; // pack id -> its icon, extracted to the cache
    QHash<QString, QString> ports;
    bool portsOk = false;
    int outstanding = 0;
};

// Steps that each call their `next` when their answer is in, run one after
// another, then `done`. packd serves one request at a time, so a queue of
// requests is the shape every refresh asks it in.
using Chained = std::function<void(std::function<void()>)>;

static void runInOrder(const std::shared_ptr<QList<Chained>> &steps, const std::function<void()> &done)
{
    if (steps->isEmpty()) {
        done();
        return;
    }
    const Chained step = steps->takeFirst();
    step([steps, done] {
        runInOrder(steps, done);
    });
}

// Pack icons, extracted once per pack version: a new version is a new file
// name, so an icon changed by an update is never the old one from the cache.
static QString iconCacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u"/kdos-discover/icons"_s;
}

static QString cachedIcon(const QString &id, const QString &version)
{
    const QDir dir(iconCacheDir());
    for (const auto *ext : {".png", ".svg"}) {
        const QString path = dir.filePath(id + u'-' + version + QLatin1String(ext));
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

// What `kdos-pack extract-meta --icon` printed, written under the extension
// its bytes are; anything that is neither PNG nor SVG is no icon.
static QString storeIcon(const QString &id, const QString &version, const QByteArray &bytes)
{
    QString ext;
    if (bytes.startsWith("\x89PNG\r\n\x1a\n"))
        ext = u".png"_s;
    else if (bytes.left(512).contains("<svg"))
        ext = u".svg"_s;
    else
        return {};
    QDir().mkpath(iconCacheDir());
    const QString path = QDir(iconCacheDir()).filePath(id + u'-' + version + ext);
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
        return {};
    return path;
}

static void runTool(QObject *owner, const QString &program, const QStringList &args, std::function<void(const QByteArray &, bool)> done)
{
    auto *p = new QProcess(owner);
    auto *timer = new QTimer(p);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, p, [p] {
        p->kill();
    });
    auto fired = std::make_shared<bool>(false);
    auto finish = [p, done, fired](bool ok) {
        if (*fired)
            return;
        *fired = true;
        done(ok ? p->readAllStandardOutput() : QByteArray(), ok);
        p->deleteLater();
    };
    QObject::connect(p, &QProcess::finished, p, [finish, p](int, QProcess::ExitStatus how) {
        finish(how == QProcess::NormalExit);
    });
    QObject::connect(p, &QProcess::errorOccurred, p, [finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(false);
    });
    p->setProcessChannelMode(QProcess::SeparateChannels);
    p->start(program, args);
    timer->start(ToolTimeoutMs);
}

KdosBackend::KdosBackend(QObject *parent)
    : AbstractResourcesBackend(parent)
    , m_updater(new StandardBackendUpdater(this))
{
    connect(m_updater, &StandardBackendUpdater::updatesCountChanged, this, &KdosBackend::updatesCountChanged);

    // Off KDOS there is nothing to ask, and Discover drops a backend that is
    // not valid instead of showing an empty source.
    m_valid = !QStandardPaths::findExecutable(u"kdos-appbox"_s).isEmpty();

    QFile cat(cataloguePath());
    if (cat.open(QIODevice::ReadOnly))
        m_catalogue = parseCatalogue(cat.readAll());
    m_context.catalogue = &m_catalogue;
    m_context.mediumPacks = mediumPacksDir();

    m_debianExclude = readDebianExclude();

    QFile pending(pendingPath());
    if (pending.open(QIODevice::ReadOnly)) {
        const Selection chosen = parseSelection(pending.readAll());
        m_chosenAtInstall = QSet<QString>(chosen.ids.cbegin(), chosen.ids.cend());
    }

    // A box created or removed in a terminal, a pack installed by
    // `kdos-appbox import`, a medium inserted: each changes one of these
    // directories, and a settled change reads the machine again.
    m_settle.setSingleShot(true);
    m_settle.setInterval(1500);
    connect(&m_settle, &QTimer::timeout, this, [this] {
        refresh();
    });
    const QStringList watched{QDir::home().filePath(u".config/kdos/boxes"_s), packStoreDir(), mediumPacksDir()};
    for (const QString &dir : watched) {
        if (QFileInfo(dir).isDir())
            m_watcher.addPath(dir);
    }
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        if (!m_running)
            m_settle.start();
    });

    loadPool();
    refresh();
}

// The tools and the packd requests still running are ended before any
// member goes: QObject deletes children only after this destructor, and a
// process ending then would deliver its answer to a backend half gone.
KdosBackend::~KdosBackend()
{
    const auto requests = findChildren<PackdRequest *>(Qt::FindDirectChildrenOnly);
    for (PackdRequest *r : requests) {
        r->disconnect();
        delete r;
    }
    const auto processes = findChildren<QProcess *>(Qt::FindDirectChildrenOnly);
    for (QProcess *p : processes) {
        p->disconnect();
        p->kill();
        p->waitForFinished(1000);
        delete p;
    }
}

bool KdosBackend::isValid() const
{
    return m_valid;
}

AbstractReviewsBackend *KdosBackend::reviewsBackend() const
{
    return nullptr;
}

AbstractBackendUpdater *KdosBackend::backendUpdater() const
{
    return m_updater;
}

int KdosBackend::updatesCount() const
{
    return m_updater->updatesCount();
}

QString KdosBackend::displayName() const
{
    return i18nc("@title the name of a software source", "KDOS");
}

bool KdosBackend::hasApplications() const
{
    return true;
}

int KdosBackend::fetchingUpdatesProgress() const
{
    return m_pending ? 50 : 100;
}

PlanContext KdosBackend::planContext() const
{
    return m_context;
}

QString KdosBackend::originName(const QString &origin)
{
    if (origin == "kdos-packs"_L1)
        return i18nc("@info:source", "KDOS packs");
    if (origin == "medium"_L1)
        return i18nc("@info:source", "KDOS medium");
    if (origin == "kdos-store"_L1)
        return i18nc("@info:source", "Debian, built in a box");
    if (origin == "kdos-boxes"_L1)
        return i18nc("@info:source", "Boxes");
    if (origin == "kdos-ports"_L1)
        return i18nc("@info:source", "KDOS system");
    if (origin == "kdos-ports-source"_L1)
        return i18nc("@info:source", "KDOS, built on this computer");
    if (origin == "local"_L1)
        return i18nc("@info:source", "File");
    return origin;
}

// ── the AppStream pool ──────────────────────────────────────────────────

void KdosBackend::loadPool()
{
    m_pool = std::make_unique<AppStream::Pool>();
    // The pool serves the Debian lane only: its search and its categories run
    // over thousands of DEP-11 components. It reads the OS catalogue, which
    // holds the kdos-ports catalogue too, and no metainfo: the host's own
    // metainfo files are what that catalogue was composed from.
    // KDOS_SWCATALOG, a colon-separated list of catalogue roots, replaces
    // the OS catalogue for a test.
    const QStringList roots = swcatalogRoots();
    if (!qEnvironmentVariableIsEmpty("KDOS_SWCATALOG")) {
        m_pool->setFlags(AppStream::Pool::FlagNone);
        for (const QString &dir : roots)
            m_pool->addExtraDataLocation(dir, AppStream::Metadata::FormatStyleCatalog);
    } else {
        m_pool->setFlags(AppStream::Pool::FlagLoadOsCatalog);
    }
    connect(m_pool.get(), &AppStream::Pool::loadFinished, this, [this](bool ok) {
        if (!ok)
            qCWarning(LIBDISCOVER_BACKEND_KDOS_LOG) << "AppStream pool:" << m_pool->lastError();
        m_debianByPackage.clear();
        const auto all = m_pool->components();
        for (const AppStream::Component &c : all) {
            if (!c.origin().startsWith("debian"_L1) || c.kind() != AppStream::Component::KindDesktopApp)
                continue;
            for (const QString &p : c.packageNames())
                m_debianByPackage.insert(p, c);
        }
        qCDebug(LIBDISCOVER_BACKEND_KDOS_LOG) << "AppStream pool:" << all.size() << "components," << m_debianByPackage.size() << "Debian packages";
        m_poolReady = true;
        refresh();
    });
    m_pool->loadAsync();
}

// The components of one catalogue file, by their first package name. Read
// whole and outside the pool: a pool keys every OS catalogue component by
// the same data id, so a host port and its Debian twin (org.kde.kate is in
// both) would be one component, whichever section loaded last.
static QHash<QString, AppStream::Component> readCatalogueFile(const QString &path)
{
    QHash<QString, AppStream::Component> out;
    if (!QFileInfo::exists(path))
        return out;
    AppStream::Metadata md;
    md.setFormatStyle(AppStream::Metadata::FormatStyleCatalog);
    if (md.parseFile(path, AppStream::Metadata::FormatKindXml) != AppStream::Metadata::MetadataErrorNoError) {
        qCWarning(LIBDISCOVER_BACKEND_KDOS_LOG) << path << md.lastError();
        return out;
    }
    const AppStream::ComponentBox box = md.components();
    for (const AppStream::Component &c : box) {
        const QString pkg = c.packageNames().value(0);
        if (!pkg.isEmpty() && !out.contains(pkg))
            out.insert(pkg, c);
    }
    return out;
}

void KdosBackend::readHostCatalogues()
{
    m_portsByPackage.clear();
    m_hostIds.clear();
    for (const QString &root : swcatalogRoots())
        m_portsByPackage.insert(readCatalogueFile(root + u"/xml/kdos-ports.xml"_s));
    for (const AppStream::Component &c : std::as_const(m_portsByPackage))
        m_hostIds.insert(c.id());
    m_binhostByPackage = readCatalogueFile(binhostCatalogue());
}

// A desktop-entry category to the Breeze icon Discover's own categories use.
static QString categoryIcon(const QString &category)
{
    static const QHash<QString, QString> icons{
        {u"AudioVideo"_s, u"applications-multimedia"_s},
        {u"Development"_s, u"applications-development"_s},
        {u"Education"_s, u"applications-education"_s},
        {u"Game"_s, u"applications-games"_s},
        {u"Graphics"_s, u"applications-graphics"_s},
        {u"Network"_s, u"applications-internet"_s},
        {u"Office"_s, u"applications-office"_s},
        {u"Science"_s, u"applications-science"_s},
        {u"Settings"_s, u"preferences-system"_s},
        {u"System"_s, u"applications-system"_s},
        {u"Utility"_s, u"applications-utilities"_s},
    };
    return icons.value(category, u"applications-other"_s);
}

// The icon of the first of the categories that has one of its own.
static QString categoryIcon(const QStringList &categories)
{
    for (const QString &c : categories) {
        const QString icon = categoryIcon(c);
        if (icon != "applications-other"_L1)
            return icon;
    }
    return u"applications-other"_s;
}

// The first stored icon file the component names, else its stock name when
// the icon theme has it: Debian's icons are not installed on the host, and
// a name the theme lacks is drawn as Discover's missing-icon fallback.
static QString iconOf(const AppStream::Component &c)
{
    QString stock;
    const auto icons = c.icons();
    for (const AppStream::Icon &icon : icons) {
        if (icon.kind() == AppStream::Icon::KindCached || icon.kind() == AppStream::Icon::KindLocal) {
            const QString path = icon.url().toLocalFile();
            if (!path.isEmpty() && QFileInfo::exists(path))
                return path;
        } else if (icon.kind() == AppStream::Icon::KindStock && stock.isEmpty() && QIcon::hasThemeIcon(icon.name())) {
            stock = icon.name();
        }
    }
    return stock;
}

void KdosBackend::enrichFromComponent(Entry *e, const AppStream::Component &c) const
{
    if (e->summary.isEmpty())
        e->summary = c.summary();
    if (!c.description().isEmpty()) {
        e->description = c.description();
        e->descriptionIsMarkup = true;
    }
    const QString icon = iconOf(c);
    if (!icon.isEmpty())
        e->icon = icon;
    if (e->homepage.isEmpty())
        e->homepage = c.url(AppStream::Component::UrlKindHomepage).toString();
    if (e->license.isEmpty())
        e->license = c.projectLicense();
    const auto provided = c.provided(AppStream::Provided::KindMimetype);
    if (!provided.isEmpty())
        e->mimetypes = provided.items();
    for (const QString &cat : c.categories()) {
        if (!e->categories.contains(cat))
            e->categories << cat;
    }
    if (e->author.isEmpty())
        e->author = c.developer().name();
    const auto launchable = c.launchable(AppStream::Launchable::KindDesktopId);
    if (!launchable.entries().isEmpty())
        e->desktopId = launchable.entries().first();
    // A screenshot is shown only when it is a file on this machine: a remote
    // one would be a download nobody asked for.
    const Screenshots shots = AppStreamUtils::fetchScreenshots(c);
    for (const Screenshot &s : shots) {
        if (s.screenshot.isLocalFile() && QFileInfo::exists(s.screenshot.toLocalFile()))
            e->screenshots << s.screenshot.toLocalFile();
    }
}

Entry KdosBackend::describeDebian(const AppStream::Component &c) const
{
    Entry e;
    e.lane = Lane::Debian;
    e.packages = {c.packageNames().value(0)};
    e.id = e.packages.first();
    e.name = c.name();
    e.appstreamId = QString(); // kept off appstream://, which names host software
    enrichFromComponent(&e, c);
    if (e.icon.isEmpty())
        e.icon = categoryIcon(e.categories);
    const QString box = boxNameForDebian(e.id);
    const auto b = m_context.boxes.constFind(box);
    if (b != m_context.boxes.constEnd()) {
        e.state = EntryState::Installed;
        e.box = box;
        e.boxBase = b->base;
        e.boxState = b->state;
        e.boxAccent = b->accent;
        e.boxPersistence = b->persistence;
        e.categories << u"kdos-box"_s;
    }
    return e;
}

KdosResource *KdosBackend::debianResource(const AppStream::Component &c)
{
    const Entry e = describeDebian(c);
    const QString key = KdosResource::keyOf(e);
    if (auto *have = m_resources.value(key)) {
        have->setEntry(e);
        return have;
    }
    auto *res = new KdosResource(e, this);
    m_resources.insert(key, res);
    return res;
}

QList<KdosResource *> KdosBackend::debianFor(const Filters &filter)
{
    QList<KdosResource *> out;
    if (!m_poolReady || (!filter.search.isEmpty() && filter.search.size() < 2))
        return out;
    if (filter.search.isEmpty() && !filter.category)
        return out;
    if (!filter.origin.isEmpty() && filter.origin != "kdos-store"_L1)
        return out;

    QSet<QString> claimed;
    for (const CatalogueRow &row : std::as_const(m_catalogue.rows)) {
        for (const QString &p : row.packages)
            claimed.insert(p);
    }

    const AppStream::ComponentBox box = filter.search.isEmpty() ? m_pool->componentsByCategories(filter.category->involvedCategories())
                                                                : m_pool->search(filter.search);
    for (const AppStream::Component &c : box) {
        if (out.size() >= DebianQueryLimit)
            break;
        if (!c.origin().startsWith("debian"_L1) || c.kind() != AppStream::Component::KindDesktopApp)
            continue;
        // A program the host has natively is never offered again in a box.
        const QString pkg = c.packageNames().value(0);
        if (!isDebianPackage(pkg) || claimed.contains(pkg) || m_hostIds.contains(c.id()) || m_debianExclude.contains(pkg))
            continue;
        out << debianResource(c);
    }
    return out;
}

// ── reading the machine ─────────────────────────────────────────────────

void KdosBackend::refresh(std::function<void()> then)
{
    // A refresh asked for while one runs is a second refresh: the running one
    // may have read the machine before the change its caller made, so the
    // caller's callback waits for the one after it.
    if (m_pending) {
        m_refreshAgain = true;
        if (then)
            m_afterNextRefresh << then;
        return;
    }
    if (then)
        m_afterRefresh << then;
    m_afterRefresh << std::exchange(m_afterNextRefresh, {});
    m_pending = 1;
    Q_EMIT fetchingUpdatesProgressChanged();

    auto snap = std::make_shared<Snapshot>();
    auto landed = [this, snap] {
        if (--snap->outstanding > 0)
            return;
        apply(*snap);
        m_initialized = true;
        m_pending = 0;
        Q_EMIT fetchingUpdatesProgressChanged();
        auto callbacks = std::exchange(m_afterRefresh, {});
        if (m_poolReady)
            callbacks << std::exchange(m_afterPool, {});
        if (m_refreshAgain) {
            m_refreshAgain = false;
            refresh();
        }
        for (const auto &cb : callbacks)
            cb();
    };

    snap->outstanding = 6;
    runTool(this, u"kdos-appbox"_s, {u"catalogue"_s}, [snap, landed](const QByteArray &out, bool ok) {
        snap->listing = parseCatalogueListing(out);
        snap->listingOk = ok;
        landed();
    });
    runTool(this, u"kdos-appbox"_s, {u"catalogue"_s, u"--groups"_s}, [snap, landed](const QByteArray &out, bool) {
        snap->groups = parseGroupListing(out);
        landed();
    });
    runTool(this, u"kdos-appbox"_s, {u"catalogue"_s, u"--chain"_s}, [snap, landed](const QByteArray &out, bool) {
        snap->chain = parseChainListing(out);
        landed();
    });
    runTool(this, u"kdos-box"_s, {u"list"_s, u"--json"_s}, [snap, landed](const QByteArray &out, bool) {
        snap->boxes = parseBoxList(out);
        landed();
    });
    runTool(this, u"kpkg"_s, {u"list"_s, u"--json"_s}, [snap, landed](const QByteArray &out, bool ok) {
        snap->ports = parseKpkgList(out);
        snap->portsOk = ok;
        landed();
    });

    // packd's list, then an `info` for each pack no catalogue row describes,
    // then each pack's icon out of its file: the store copy where there is
    // one, else the medium's.
    auto *req = PackdRequest::send(u"list"_s, this, 15000);
    connect(req, &PackdRequest::finished, this, [this, snap, landed](const QByteArray &reply, const QString &error) {
        QString why = error;
        if (why.isEmpty() && !replyOk(reply, &why))
            snap->packdError = why;
        else if (!why.isEmpty())
            snap->packdError = why;
        snap->packs = parsePackList(reply);

        auto steps = std::make_shared<QList<Chained>>();
        QSet<QString> asked;
        for (const PackRow &p : std::as_const(snap->packs)) {
            if (isOffered(p.id) || asked.contains(p.id))
                continue;
            asked.insert(p.id);
            const QString id = p.id;
            *steps << [this, snap, id](std::function<void()> next) {
                auto *info = PackdRequest::send(u"info "_s + id, this, 15000);
                connect(info, &PackdRequest::finished, this, [snap, id, next](const QByteArray &r, const QString &err) {
                    if (err.isEmpty() && replyOk(r))
                        snap->packMeta.insert(id, parseMeta(r));
                    next();
                });
            };
        }
        QHash<QString, PackRow> iconFrom;
        for (const PackRow &p : std::as_const(snap->packs)) {
            if (!iconFrom.contains(p.id) || p.origin == "store"_L1)
                iconFrom.insert(p.id, p);
        }
        for (const PackRow &p : std::as_const(iconFrom)) {
            const QString dir = p.origin == "store"_L1 ? packStoreDir() : mediumPacksDir();
            const QString file = QDir(dir).filePath(p.id + u".kpack"_s);
            const QString id = p.id, version = p.version;
            *steps << [this, snap, id, version, file](std::function<void()> next) {
                const QString have = cachedIcon(id, version);
                if (!have.isEmpty() || !QFileInfo::exists(file)) {
                    if (!have.isEmpty())
                        snap->packIcons.insert(id, have);
                    next();
                    return;
                }
                runTool(this, u"kdos-pack"_s, {u"extract-meta"_s, file, u"--icon"_s}, [snap, id, version, next](const QByteArray &out, bool ok) {
                    const QString path = ok ? storeIcon(id, version, out) : QString();
                    if (!path.isEmpty())
                        snap->packIcons.insert(id, path);
                    next();
                });
            };
        }
        runInOrder(steps, landed);
    });
}

// A catalogue row `kdos-appbox catalogue` lists: an application or a data
// set. Its pack is shown on its row; any other pack is a resource of its own.
bool KdosBackend::isOffered(const QString &id) const
{
    const auto row = m_catalogue.rows.constFind(id);
    return row != m_catalogue.rows.constEnd() && (row->kind == "app"_L1 || row->kind == "data"_L1);
}

KdosResource *KdosBackend::upsert(const Entry &entry, QSet<QString> *seen)
{
    const QString key = KdosResource::keyOf(entry);
    seen->insert(key);
    if (auto *have = m_resources.value(key)) {
        have->setEntry(entry);
        return have;
    }
    auto *res = new KdosResource(entry, this);
    m_resources.insert(key, res);
    return res;
}

static QString titleCase(const QString &id)
{
    return id.isEmpty() ? id : id.left(1).toUpper() + id.mid(1);
}

static void applyBox(Entry *e, const BoxRow &b)
{
    e->box = b.name;
    e->boxBase = b.base;
    e->boxState = b.state;
    e->boxAccent = b.accent;
    e->boxPersistence = b.persistence;
    if (!e->categories.contains("kdos-box"_L1))
        e->categories << u"kdos-box"_s;
}

void KdosBackend::apply(const Snapshot &snap)
{
    // What the plans are made against.
    QHash<QString, PackRow> stored, medium;
    for (const PackRow &p : snap.packs) {
        if (p.origin == "store"_L1)
            stored.insert(p.id, p);
        else
            medium.insert(p.id, p);
    }
    m_context.packs = medium;
    for (auto it = stored.cbegin(); it != stored.cend(); ++it)
        m_context.packs.insert(it.key(), it.value());
    // A store box over its own image, `image:kdos/<name>`, has a container
    // from its install on (kdos-appbox create runs podman create), so one
    // listed "not created" is the profile an uninstall keeps. It is no box
    // here: counted, it would mark its uninstalled application Installed
    // (the Debian lane) or keep it on the Boxes page (the catalogue lane).
    m_context.boxes.clear();
    QList<BoxRow> boxes;
    for (const BoxRow &b : snap.boxes) {
        if (b.state == "not created"_L1 && b.base == u"image:kdos/"_s + b.name)
            continue;
        boxes << b;
        m_context.boxes.insert(b.name, b);
    }

    QSet<QString> seen;
    QSet<QString> claimedBoxes;
    QHash<QString, bool> installed;
    QHash<QString, QStringList> groupsOf;
    for (const CatalogueGroup &g : snap.groups) {
        for (const QString &m : g.members)
            groupsOf[m] << g.id;
    }

    // ── catalogue rows ──
    for (const CatalogueListing &l : snap.listing) {
        Entry e;
        e.lane = Lane::Catalogue;
        e.id = l.id;
        const CatalogueRow row = m_catalogue.rows.value(l.id);
        e.kind = row.kind.isEmpty() ? u"app"_s : row.kind;
        e.name = l.name.isEmpty() ? l.id : l.name;
        e.summary = l.tagline;
        e.categories = {l.category};
        // The installer records groups, and a person picks from the page one
        // application at a time as well as one set at a time.
        bool pending = m_chosenAtInstall.contains(l.id);
        for (const QString &g : groupsOf.value(l.id)) {
            e.categories << u"kdos-set:"_s + g;
            pending |= m_chosenAtInstall.contains(g);
        }
        if (pending)
            e.categories << u"kdos-pending"_s;
        e.packages = row.packages;
        e.size = l.bytes;
        e.sizeIsEstimate = true;
        e.icon = categoryIcon(l.category);

        const auto s = stored.constFind(l.id);
        const auto m = medium.constFind(l.id);
        const bool inStore = s != stored.constEnd();
        if (inStore) {
            e.installedVersion = s->version;
            e.availableVersion = s->version;
            e.size = s->size;
            e.sizeIsEstimate = false;
        }
        if (m != medium.constEnd()) {
            e.availableVersion = m->version;
            if (!inStore) {
                e.size = m->size;
                e.sizeIsEstimate = false;
            }
        }
        e.packOrigin = inStore ? u"store"_s : (m != medium.constEnd() ? u"medium"_s : QString());
        // A row with no pack anywhere is built here, and the first build on a
        // base or runtime builds that image too: its measured size counts.
        if (e.packOrigin.isEmpty() && !l.installed)
            e.size += unbuiltChainBytes(snap.chain, l.parent);

        const auto b = m_context.boxes.constFind(l.id);
        if (b != m_context.boxes.constEnd()) {
            applyBox(&e, *b);
            claimedBoxes.insert(l.id);
        }
        const bool here = l.installed || (inStore && e.kind == "data"_L1);
        if (here && inStore && m != medium.constEnd() && compareVersions(m->version, s->version) > 0)
            e.state = EntryState::Upgradeable;
        else
            e.state = here ? EntryState::Installed : EntryState::None;
        installed.insert(l.id, here);

        for (const QString &p : row.packages) {
            const auto c = m_debianByPackage.constFind(p);
            if (c != m_debianByPackage.constEnd()) {
                enrichFromComponent(&e, *c);
                break;
            }
        }
        // The pack's own icon is the one its box shows; Debian's is the
        // package's before KDOS packed it.
        if (snap.packIcons.contains(l.id))
            e.icon = snap.packIcons.value(l.id);
        upsert(e, &seen);
    }

    // ── sets ──
    for (const CatalogueGroup &g : snap.groups) {
        Entry e;
        e.lane = Lane::Set;
        e.id = g.id;
        e.name = i18nc("@title a catalogue group installed as one", "%1 set", titleCase(g.id));
        e.summary = g.description;
        e.members = g.members;
        e.icon = u"package-x-generic"_s;
        e.categories = {u"kdos-set"_s, u"kdos-set:"_s + g.id};
        if (m_chosenAtInstall.contains(g.id))
            e.categories << u"kdos-pending"_s;
        e.sizeIsEstimate = true;
        bool all = !g.members.isEmpty();
        // Each base or runtime still to build counts once, however many of
        // the set's applications sit on it.
        QHash<QString, ChainRow> chain = snap.chain;
        for (const QString &mid : g.members) {
            all &= installed.value(mid);
            for (const CatalogueListing &l : snap.listing) {
                if (l.id != mid)
                    continue;
                e.size += l.bytes;
                if (l.installed || stored.contains(l.id) || medium.contains(l.id))
                    continue;
                e.size += unbuiltChainBytes(chain, l.parent);
                for (QString at = l.parent; chain.contains(at) && !chain[at].built; at = chain[at].parent)
                    chain[at].built = true;
            }
        }
        e.state = all ? EntryState::Installed : EntryState::None;
        upsert(e, &seen);
    }

    // ── packs no catalogue listing shows: their own, and the bases and
    // runtimes the catalogue's applications are composed over ──
    QSet<QString> packIds;
    for (const PackRow &p : snap.packs) {
        if (!isOffered(p.id))
            packIds.insert(p.id);
    }
    for (const QString &id : std::as_const(packIds)) {
        const Meta meta = snap.packMeta.value(id);
        const auto s = stored.constFind(id);
        const auto m = medium.constFind(id);
        const PackRow &row = s != stored.constEnd() ? *s : *m;
        Entry e;
        e.lane = Lane::Pack;
        e.id = id;
        e.kind = row.kind;
        e.name = metaValue(meta, u"name"_s);
        if (e.name.isEmpty())
            e.name = id;
        e.summary = metaValue(meta, u"summary"_s);
        e.description = meta.value(u"description"_s).join(u'\n');
        e.license = metaValue(meta, u"licence"_s);
        e.mimetypes = meta.value(u"mime"_s);
        const QString cat = metaValue(meta, u"category"_s);
        e.categories = {cat.isEmpty() ? u"Other"_s : firstCategory(cat)};
        e.icon = snap.packIcons.value(id, row.kind == "app"_L1 ? categoryIcon(e.categories.first()) : u"package-x-generic"_s);
        e.packOrigin = row.origin;
        e.size = row.size;
        if (s != stored.constEnd()) {
            e.installedVersion = s->version;
            e.availableVersion = s->version;
        }
        if (m != medium.constEnd())
            e.availableVersion = m->version;

        const QString boxName = boxNameForPack(id);
        const auto b = m_context.boxes.constFind(boxName);
        if (b != m_context.boxes.constEnd()) {
            applyBox(&e, *b);
            claimedBoxes.insert(boxName);
        }
        if (s == stored.constEnd())
            e.state = EntryState::None;
        else if (m != medium.constEnd() && compareVersions(m->version, s->version) > 0)
            e.state = EntryState::Upgradeable;
        else if (row.kind == "app"_L1)
            e.state = b != m_context.boxes.constEnd() ? EntryState::Installed : EntryState::None;
        else
            e.state = EntryState::Installed;
        upsert(e, &seen);
    }

    // ── Debian boxes, eagerly: their resource is what the Installed page lists ──
    for (const BoxRow &b : std::as_const(boxes)) {
        if (!b.name.startsWith("deb."_L1))
            continue;
        QString pkg = b.name.mid(4);
        pkg.replace(u'_', u'+');
        claimedBoxes.insert(b.name);
        const auto c = m_debianByPackage.constFind(pkg);
        if (c != m_debianByPackage.constEnd()) {
            seen.insert(KdosResource::keyOf(debianResource(*c)->entry()));
            continue;
        }
        Entry e;
        e.lane = Lane::Debian;
        e.id = pkg;
        e.packages = {pkg};
        e.name = pkg;
        e.icon = categoryIcon(QString());
        e.state = EntryState::Installed;
        applyBox(&e, b);
        upsert(e, &seen);
    }

    // ── boxes nothing above accounts for ──
    for (const BoxRow &b : std::as_const(boxes)) {
        if (claimedBoxes.contains(b.name))
            continue;
        Entry e;
        e.lane = Lane::Box;
        e.id = b.name;
        e.name = b.name;
        e.summary = b.base == "-"_L1 ? i18n("A box with no recorded base") : i18n("A box on %1", b.base);
        e.icon = u"utilities-terminal"_s;
        e.state = EntryState::Installed;
        e.size = b.disk;
        applyBox(&e, b);
        upsert(e, &seen);
    }

    // ── host ports ──
    // What is installed comes from the image's catalogue and kpkg's versions;
    // what a binhost could add, from its catalogue, only for a package kpkg
    // answered is absent, so a kpkg that did not answer offers nothing. Both
    // catalogues are read again here: kdos-ports-helper rewrites the first
    // after every install, and `kdos update fetch` replaces the second.
    readHostCatalogues();
    {
        auto portEntry = [this](const QString &pkg, const AppStream::Component &c) {
            Entry e;
            e.lane = Lane::Port;
            e.id = pkg;
            e.name = c.name().isEmpty() ? pkg : c.name();
            e.appstreamId = c.id();
            enrichFromComponent(&e, c);
            if (e.icon.isEmpty())
                e.icon = u"application-x-executable"_s;
            return e;
        };
        for (auto it = m_portsByPackage.cbegin(); it != m_portsByPackage.cend(); ++it) {
            if (!isSafeId(it.key()))
                continue;
            Entry e = portEntry(it.key(), *it);
            const QString version = snap.ports.value(it.key());
            e.installedVersion = version.isEmpty() ? it->customValue(u"KDOS::Version"_s) : version;
            e.availableVersion = e.installedVersion;
            e.state = EntryState::Installed;
            upsert(e, &seen);
        }
        if (snap.portsOk) {
            for (auto it = m_binhostByPackage.cbegin(); it != m_binhostByPackage.cend(); ++it) {
                if (!isSafeId(it.key()) || m_portsByPackage.contains(it.key()))
                    continue;
                Entry e = portEntry(it.key(), *it);
                const QString version = snap.ports.value(it.key());
                e.availableVersion = it->customValue(u"KDOS::Version"_s);
                e.installedVersion = version;
                e.state = version.isEmpty() ? EntryState::None : EntryState::Installed;
                upsert(e, &seen);
                // The same port, built here: a second origin in Discover's
                // source picker, offered only while it is not installed, so
                // an installed port is one resource.
                if (e.state == EntryState::None) {
                    e.lane = Lane::PortSource;
                    upsert(e, &seen);
                }
            }
        }
    }

    // ── .ktar sets on the medium ──
    const QDir mediumDir(mediumPacksDir());
    const QStringList ktars = mediumDir.entryList({u"*.ktar"_s}, QDir::Files);
    for (const QString &f : ktars)
        m_files.insert(mediumDir.filePath(f));

    // Debian resources made by a search are kept, with their box state read
    // again; anything else a refresh did not see has left the machine.
    QList<KdosResource *> gone;
    for (auto it = m_resources.begin(); it != m_resources.end();) {
        KdosResource *res = it.value();
        const Lane lane = res->entry().lane;
        if (seen.contains(it.key()) || lane == Lane::Archive || lane == Lane::PackFile) {
            ++it;
            continue;
        }
        if (lane == Lane::Debian && m_poolReady) {
            const auto c = m_debianByPackage.constFind(res->entry().id);
            if (c != m_debianByPackage.constEnd()) {
                res->setEntry(describeDebian(*c));
                ++it;
                continue;
            }
        }
        gone << res;
        it = m_resources.erase(it);
    }
    for (KdosResource *res : std::as_const(gone)) {
        Q_EMIT resourceRemoved(res);
        res->deleteLater();
    }

    // A .ktar is read again so its state follows its members; the stream each
    // read returns finishes and deletes itself unread.
    const QSet<QString> files = m_files;
    for (const QString &file : files) {
        if (file.endsWith(".ktar"_L1) && QFileInfo::exists(file))
            resourceForFile(QUrl::fromLocalFile(file));
    }

    if (!snap.packdError.isEmpty())
        qCDebug(LIBDISCOVER_BACKEND_KDOS_LOG) << "kdos-packd:" << snap.packdError;
    // The first box made, or the last one removed, adds or drops the Boxes
    // page: Discover asks for the categories again.
    if (m_categoriesRead && !m_context.boxes.isEmpty() != m_listedBoxes) {
        m_listedBoxes = !m_context.boxes.isEmpty();
        QTimer::singleShot(0, CategoryModel::global(), &CategoryModel::populateCategories);
    }
    m_context.mediumPacks = mediumPacksDir();
    Q_EMIT contentsChanged();
    Q_EMIT updatesCountChanged();
}

// ── search ──────────────────────────────────────────────────────────────

static uint matchScore(KdosResource *res, const QString &term)
{
    if (term.isEmpty())
        return 1;
    if (res->name().compare(term, Qt::CaseInsensitive) == 0)
        return 100;
    if (res->name().startsWith(term, Qt::CaseInsensitive))
        return 80;
    if (res->name().contains(term, Qt::CaseInsensitive))
        return 60;
    if (res->packageName().contains(term, Qt::CaseInsensitive))
        return 40;
    if (res->comment().contains(term, Qt::CaseInsensitive))
        return 20;
    return 0;
}

ResultsStream *KdosBackend::search(const AbstractResourcesBackend::Filters &filter)
{
    if (!filter.resourceUrl.isEmpty())
        return resourceForUrl(filter.resourceUrl);

    auto *stream = new ResultsStream(u"KdosStream"_s);
    auto answer = [this, stream, filter] {
        QVector<StreamResult> found;
        QSet<KdosResource *> have;
        const bool source = preferSource();
        auto consider = [&](KdosResource *res) {
            if (have.contains(res))
                return;
            const uint score = matchScore(res, filter.search);
            if (score == 0 || !filter.shouldFilter(res))
                return;
            have.insert(res);
            found << StreamResult{res, score};
        };
        for (KdosResource *res : std::as_const(m_resources)) {
            const Entry &e = res->entry();
            // A file somebody opened shows up as that file's page, never in a
            // search; a set on the medium is part of what the machine offers.
            if ((e.lane == Lane::Archive && e.packOrigin != "medium"_L1) || e.lane == Lane::PackFile)
                continue;
            // A list browsed by origin names every resource of that origin.
            if (filter.origin.isEmpty() && isSecondTwin(res, source))
                continue;
            consider(res);
        }
        if (filter.state < AbstractResource::Installed) {
            for (KdosResource *res : debianFor(filter))
                consider(res);
        }
        if (!found.isEmpty())
            Q_EMIT stream->resourcesFound(found);
        stream->finish();
    };
    // Until the machine has been read once there is nothing to answer from;
    // after that a search answers from what the last refresh saw.
    if (!m_initialized)
        m_afterRefresh << answer;
    else
        QTimer::singleShot(0, stream, answer);
    return stream;
}

ResultsStream *KdosBackend::resourceForUrl(const QUrl &url)
{
    if (url.isLocalFile())
        return resourceForFile(url);

    // A link is answered from what the machine holds, so it waits for the
    // first refresh, and an appstream:// link for the AppStream pool too:
    // Discover opened on a link starts the backend with nothing read, and an
    // answer then would be "no entry" for an application that is there.
    auto *stream = new ResultsStream(u"KdosStream-url"_s);
    QPointer<ResultsStream> guard(stream);
    auto answer = [this, guard, url] {
        if (!guard)
            return;
        const QVector<StreamResult> found = resourcesForUrl(url);
        if (!found.isEmpty())
            Q_EMIT guard->resourcesFound(found);
        guard->finish();
    };
    const bool needsPool = url.scheme() == "appstream"_L1;
    if (m_initialized && (m_poolReady || !needsPool))
        QTimer::singleShot(0, stream, answer);
    else if (needsPool)
        m_afterPool << answer;
    else
        m_afterRefresh << answer;
    return stream;
}

QVector<StreamResult> KdosBackend::resourcesForUrl(const QUrl &url)
{
    QVector<StreamResult> found;
    if (url.scheme() == "appstream"_L1) {
        const QStringList ids = AppStreamUtils::appstreamIds(url);
        // The entry's own id: appstreamId() makes one up for a resource
        // without, which no link names.
        for (KdosResource *res : std::as_const(m_resources)) {
            const QString asid = res->entry().appstreamId;
            if (!asid.isEmpty() && ids.contains(asid, Qt::CaseInsensitive))
                found << StreamResult{res, 0};
        }
        // An appstream:// link to an application only Debian's DEP-11 knows,
        // as KRunner's software results give: the catalogue row that packs
        // its package when there is one, else the Debian lane's resource.
        if (found.isEmpty() && m_poolReady) {
            for (const QString &id : ids) {
                if (m_hostIds.contains(id))
                    continue;
                const AppStream::ComponentBox box = m_pool->componentsById(id);
                for (const AppStream::Component &c : box) {
                    const QString pkg = c.packageNames().value(0);
                    if (!c.origin().startsWith("debian"_L1) || !isDebianPackage(pkg))
                        continue;
                    KdosResource *res = nullptr;
                    for (const CatalogueRow &row : std::as_const(m_catalogue.rows)) {
                        if (row.packages.contains(pkg)) {
                            res = m_resources.value(u"catalogue/"_s + row.id);
                            break;
                        }
                    }
                    if (!res && c.kind() == AppStream::Component::KindDesktopApp && !m_debianExclude.contains(pkg))
                        res = debianResource(c);
                    if (res && !found.contains(StreamResult{res, 0}))
                        found << StreamResult{res, 0};
                }
            }
        }
    } else if (url.scheme() == name()) {
        const QString key = url.host() + u'/' + url.path().mid(1);
        if (auto *res = m_resources.value(key))
            found << StreamResult{res, 0};
    }
    const bool source = preferSource();
    std::stable_partition(found.begin(), found.end(), [this, source](const StreamResult &r) {
        return !isSecondTwin(static_cast<KdosResource *>(r.resource), source);
    });
    return found;
}

// A port not installed is two resources with one AppStream id, the binhost's
// and the one built here. Discover's lists keep one resource per id, and
// which of two equal rows survives its sort no backend decides; the
// application page then offers the rest in its source picker, which it
// fills by asking for the application's URL, and a link opens the first
// resource that answers. So a list names only the twin `install =` prefers,
// and a URL answers both, that one first: the other is the second twin.
bool KdosBackend::isSecondTwin(const KdosResource *res, bool preferSource) const
{
    const Entry &e = res->entry();
    if (e.lane == Lane::PortSource)
        return !preferSource;
    if (e.lane == Lane::Port && preferSource && e.state == EntryState::None)
        return m_resources.contains(u"port-source/"_s + e.id);
    return false;
}

ResultsStream *KdosBackend::resourceForFile(const QUrl &url)
{
    const QString path = url.toLocalFile();
    const QFileInfo fi(path);
    if (!fi.isFile() || !(path.endsWith(".ktar"_L1) || path.endsWith(".kpack"_L1)))
        return new ResultsStream(u"KdosStream-file"_s, {});
    m_files.insert(fi.absoluteFilePath());

    if (path.endsWith(".ktar"_L1)) {
        KTar tar(fi.absoluteFilePath());
        Entry e;
        e.lane = Lane::Archive;
        e.path = fi.absoluteFilePath();
        e.id = fi.fileName();
        e.icon = u"package-x-generic"_s;
        e.categories = {u"kdos-set"_s};
        if (fi.absolutePath() == QDir(mediumPacksDir()).absolutePath())
            e.packOrigin = u"medium"_s;
        if (tar.open(QIODevice::ReadOnly)) {
            const KArchiveDirectory *root = tar.directory();
            auto read = [root](const QString &name) -> QByteArray {
                const auto *f = dynamic_cast<const KArchiveFile *>(root->entry(name));
                return f ? f->data() : QByteArray();
            };
            const Selection sel = parseSelection(read(u"SELECTION"_s));
            const QList<IndexRow> index = parsePackIndex(read(u"PACKAGES"_s));
            const bool signedIndex = root->entry(u"PACKAGES.sig"_s) != nullptr;
            if (sel.ids.isEmpty()) {
                // A tar with no SELECTION naming an application is no set:
                // its page says so, and installPlan refuses it.
                e.name = fi.fileName();
                e.summary = i18n("This file is not a KDOS set.");
                e.signature = u"refused"_s;
                QSet<QString> seen;
                auto *res = upsert(e, &seen);
                return new ResultsStream(u"KdosStream-ktar"_s, {StreamResult{res, 0}});
            }
            e.members = sel.ids;
            QStringList names;
            for (const QString &id : sel.ids) {
                QString shown = id;
                for (const IndexRow &r : index) {
                    if (r.id == id) {
                        shown = r.name.isEmpty() ? id : r.name;
                        e.size += r.size;
                    }
                }
                names << shown;
            }
            e.name = sel.groups.isEmpty() ? fi.completeBaseName() : i18nc("@title a .ktar named by its sets", "%1 (%2)", fi.completeBaseName(), sel.groups.join(u", "_s));
            e.summary = i18ncp("@info", "One application to import", "%1 applications to import", int(sel.ids.size()));
            e.description = i18n("Imported through kdos-packd, which checks every pack before it is installed.") + u"\n\n"_s + names.join(u'\n');
            e.signature = signedIndex ? u"signed-index"_s : u"unsigned"_s;
            bool all = !sel.ids.isEmpty();
            for (const QString &id : sel.ids) {
                const auto r = m_resources.value(u"catalogue/"_s + id);
                all &= (r && r->entry().state != EntryState::None) || m_context.boxes.contains(boxNameForPack(id));
            }
            e.state = all ? EntryState::Installed : EntryState::None;
        } else {
            e.name = fi.fileName();
            e.summary = i18n("This file is not a readable .ktar archive.");
            e.signature = u"refused"_s;
        }
        QSet<QString> seen;
        auto *res = upsert(e, &seen);
        return new ResultsStream(u"KdosStream-ktar"_s, {StreamResult{res, 0}});
    }

    // A .kpack: its metadata and its signature come from kdos-pack, which
    // carries the only reader of the format.
    auto *stream = new ResultsStream(u"KdosStream-kpack"_s);
    const QString abs = fi.absoluteFilePath();
    runTool(this, u"kdos-pack"_s, {u"extract-meta"_s, abs}, [this, stream, abs](const QByteArray &metaOut, bool ok) {
        // A file kdos-pack cannot read is still a page, one that says what
        // it is not; an empty answer would drop Discover back on Home.
        if (!ok || metaOut.isEmpty()) {
            Entry e;
            e.lane = Lane::PackFile;
            e.path = abs;
            e.name = QFileInfo(abs).fileName();
            e.summary = i18n("This file is not a KDOS pack, or it is damaged.");
            e.icon = u"package-x-generic"_s;
            e.signature = u"refused"_s;
            e.size = quint64(QFileInfo(abs).size());
            QSet<QString> seen;
            auto *res = upsert(e, &seen);
            Q_EMIT stream->resourcesFound({StreamResult{res, 0}});
            stream->finish();
            return;
        }
        const Meta meta = parseMeta(metaOut);
        runTool(this, u"kdos-pack"_s, {u"verify"_s, abs}, [this, stream, abs, meta](const QByteArray &verifyOut, bool) {
            Entry e;
            e.lane = Lane::PackFile;
            e.path = abs;
            e.id = metaValue(meta, u"id"_s);
            e.kind = metaValue(meta, u"kind"_s);
            e.name = metaValue(meta, u"name"_s);
            if (e.name.isEmpty())
                e.name = e.id;
            e.summary = metaValue(meta, u"summary"_s);
            e.description = meta.value(u"description"_s).join(u'\n');
            e.license = metaValue(meta, u"licence"_s);
            // version-release, the form packd lists a stored copy in.
            const QString release = metaValue(meta, u"release"_s);
            e.availableVersion = metaValue(meta, u"version"_s) + (release.isEmpty() ? QString() : u'-' + release);
            const QString cat = metaValue(meta, u"category"_s);
            e.categories = {cat.isEmpty() ? u"Other"_s : firstCategory(cat)};
            e.icon = e.kind == "app"_L1 ? categoryIcon(e.categories.first()) : u"package-x-generic"_s;
            e.size = quint64(QFileInfo(abs).size());
            // `<id>: signed by <key>` or `<id>: unsigned` on standard output;
            // a refusal goes to standard error and leaves this empty.
            const QString v = QString::fromUtf8(verifyOut).trimmed();
            const qsizetype by = v.indexOf(": signed by "_L1);
            e.signature = by >= 0 ? v.mid(by + 12).trimmed() : v.endsWith(": unsigned"_L1) ? u"unsigned"_s : u"refused"_s;
            const auto stored = m_context.packs.constFind(e.id);
            if (stored != m_context.packs.constEnd() && stored->origin == "store"_L1) {
                e.installedVersion = stored->version;
                e.state = compareVersions(e.availableVersion, stored->version) > 0 ? EntryState::Upgradeable : EntryState::Installed;
            }
            const QString cached = isSafeId(e.id) ? cachedIcon(e.id, e.availableVersion) : QString();
            auto publish = [this, stream](Entry entry) {
                QSet<QString> seen;
                auto *res = upsert(entry, &seen);
                Q_EMIT stream->resourcesFound({StreamResult{res, 0}});
                stream->finish();
            };
            if (!cached.isEmpty() || !isSafeId(e.id)) {
                if (!cached.isEmpty())
                    e.icon = cached;
                publish(e);
                return;
            }
            runTool(this, u"kdos-pack"_s, {u"extract-meta"_s, abs, u"--icon"_s}, [e, publish](const QByteArray &icon, bool ok) mutable {
                const QString path = ok ? storeIcon(e.id, e.availableVersion, icon) : QString();
                if (!path.isEmpty())
                    e.icon = path;
                publish(e);
            });
        });
    });
    return stream;
}

// ── categories ──────────────────────────────────────────────────────────

QList<std::shared_ptr<Category>> KdosBackend::category() const
{
    QFile file(u":/kdos/kdos-backend-categories.xml"_s);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray xml = file.readAll();

    // One subcategory of Sets per catalogue group, so a group added to the
    // catalogue is a page here without a change to this file.
    QByteArray sets;
    for (const CatalogueGroup &g : m_catalogue.groups) {
        if (!isSafeId(g.id))
            continue;
        const QString title = i18nc("@title a catalogue group installed as one", "%1 set", titleCase(g.id)).toHtmlEscaped();
        sets += "<Menu><Name>" + title.toUtf8() + "</Name><Icon>package-x-generic</Icon><Include><Category>kdos-set:" + g.id.toUtf8()
            + "</Category></Include></Menu>\n";
    }
    xml.replace("<!-- kdos-sets -->", sets);
    // The Boxes page exists only while a box does. Discover reads the
    // categories once, when the backend loads and before the first refresh
    // lands, so a box profile on disk counts as well; apply() has Discover
    // read them again when the answer changes.
    m_listedBoxes = hasBoxes();
    m_categoriesRead = true;
    if (m_listedBoxes) {
        xml.replace("<!-- kdos-boxes -->",
                    "<Menu><Name>Boxes</Name><Icon>utilities-terminal</Icon>"
                    "<Include><Category>kdos-box</Category></Include></Menu>");
    }
    // The page exists only while the installer's record does.
    if (!m_chosenAtInstall.isEmpty()) {
        xml.replace("<!-- kdos-pending -->",
                    "<Menu><Name>Selected during install</Name><Icon>checkmark</Icon><Top />"
                    "<Include><Category>kdos-pending</Category></Include></Menu>");
    }

    QList<std::shared_ptr<Category>> ret;
    QXmlStreamReader reader(xml);
    reader.readNextStartElement(); // the enclosing <Menu>
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == "Menu"_L1) {
            ret << std::make_shared<Category>(QSet<QString>{name()});
            ret.last()->parseData(file.fileName(), &reader, Category::Localization::No);
        }
    }
    if (reader.hasError())
        qCWarning(LIBDISCOVER_BACKEND_KDOS_LOG) << "categories:" << reader.errorString() << reader.lineNumber();
    Category::sortCategories(ret);
    return ret;
}

bool KdosBackend::hasBoxes() const
{
    if (!m_context.boxes.isEmpty())
        return true;
    if (m_initialized)
        return false;
    const QDir profiles(QDir::home().filePath(u".config/kdos/boxes"_s));
    return !profiles.entryList({u"*.conf"_s}, QDir::Files).isEmpty();
}

InlineMessage *KdosBackend::explainDysfunction() const
{
    return new InlineMessage(InlineMessage::Information,
                             u"package-x-generic"_s,
                             i18n("Nothing in the KDOS catalogue, the pack store or the system matches. A pack on a USB stick opens with Discover from the file manager."));
}

// ── transactions ────────────────────────────────────────────────────────

Transaction *KdosBackend::installApplication(AbstractResource *app)
{
    return installApplication(app, {});
}

Transaction *KdosBackend::installApplication(AbstractResource *app, const AddonList &)
{
    auto *res = qobject_cast<KdosResource *>(app);
    if (!res)
        return nullptr;
    const Plan plan = installPlan(res->entry(), m_context);
    if (!plan.isValid()) {
        Q_EMIT passiveMessage(plan.refusal);
        return nullptr;
    }
    auto *t = new KdosTransaction(res, Transaction::InstallRole, plan, this);
    enqueue(t);
    return t;
}

Transaction *KdosBackend::removeApplication(AbstractResource *app)
{
    auto *res = qobject_cast<KdosResource *>(app);
    if (!res)
        return nullptr;
    const Plan plan = removePlan(res->entry(), m_context);
    if (!plan.isValid()) {
        Q_EMIT passiveMessage(plan.refusal);
        return nullptr;
    }
    auto *t = new KdosTransaction(res, Transaction::RemoveRole, plan, this);
    enqueue(t);
    return t;
}

void KdosBackend::enqueue(KdosTransaction *transaction)
{
    m_queue << transaction;
    if (!m_running)
        QTimer::singleShot(0, this, &KdosBackend::startNext);
}

void KdosBackend::startNext()
{
    while (!m_running && !m_queue.isEmpty()) {
        QPointer<KdosTransaction> t = m_queue.takeFirst();
        if (!t || t->status() != Transaction::QueuedStatus)
            continue;
        m_running = t;
        if (!t->start())
            m_running = nullptr;
    }
}

void KdosBackend::transactionFinished(KdosTransaction *transaction, Transaction::Status status)
{
    QPointer<KdosTransaction> t(transaction);
    if (m_running == transaction)
        m_running = nullptr;
    refresh([this, t, status] {
        if (t)
            t->setStatus(status);
        startNext();
    });
}

void KdosBackend::readBoxGrants(KdosResource *resource)
{
    const QString box = resource->entry().box;
    if (!isSafeId(box))
        return;
    QPointer<KdosResource> res(resource);
    runTool(this, u"kdos-box"_s, {u"profile"_s, box}, [res, box](const QByteArray &out, bool) {
        // The resource may have left, or moved to another box, meanwhile.
        if (res && res->entry().box == box)
            res->setBoxGrants(parseBoxProfile(out));
    });
}

void KdosBackend::checkForUpdates()
{
    refresh();
}

}
