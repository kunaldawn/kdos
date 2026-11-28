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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QXmlStreamReader>

#include <algorithm>
#include <memory>
#include <tuple>

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

// The live medium's test, kdt_root_is_live()'s: `/` is an overlay. Its
// notes say an install lasts until the machine restarts, where an
// installed system's say the next update keeps it. $KDOS_MOUNTS names
// another mount table, as for kdos-bootctl.
static bool rootIsLive()
{
    const QString env = qEnvironmentVariable("KDOS_MOUNTS");
    QFile f(env.isEmpty() ? u"/proc/mounts"_s : env);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    while (!f.atEnd()) {
        const QList<QByteArray> fields = f.readLine().simplified().split(' ');
        if (fields.size() >= 3 && fields.at(1) == "/" && fields.at(2) == "overlay")
            return true;
    }
    return false;
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

// The update document `kdos update check` writes, renamed into place in
// /var/lib/kdos, which the backend watches.
static QString updateDocumentPath()
{
    const QString env = qEnvironmentVariable("KDOS_UPDATE_DOC");
    return env.isEmpty() ? u"/var/lib/kdos/update.json"_s : env;
}

// What the Updates page says above its list when the update document records
// a problem (update.c's problem(): a `fetch`, `binhost-<code>`, `image-<code>`
// or `apply` code), with where the failed step looked and the tool's own
// words: without it the page says "Up to date" about a release nothing could
// read. A tree or image step names the channel, the tree source in effect. A
// binhost step names `binhost_base`, the binhost update.conf, the environment
// or the tree's release names, then `binhost`, where the index in place came
// from, then the binary host in words; never the channel, which is another
// place, so the sentence would send the reader where the index was not
// looked for. A bare no-network or fetch-failed is the fetch's or the
// apply's, so its sentence fits either. The tool's words follow in brackets
// only when they add something: never for no-network, whose sentence says
// all the tool's does, and never when they name `origin` again, which the
// sentence already names (binhost-absent's always do). no-network is the
// code for a name that did not resolve, or for a refusal or a timeout on a
// machine with no default route (relidx.c's ri_class); a server that
// refused or did not answer while a route exists is a fetch-failed whose
// words say so, without the URL. no-tree is not said: with no tree
// nothing can be behind, and the fetch's own code says why none arrived. Nor
// is a tree fetch-failed whose detail is update.c's "the published tree … is
// older than this machine's": that machine is ahead of the release, not cut
// off from it, and would carry the warning beside every offer until the
// release caught up. update.c's published_older() knows the refusal by the
// same words, and problem() ranks it below every other code, so it is the
// document's problem only when there is no other. Empty when nothing is
// recorded or the code is unknown.
static QString updateProblemText()
{
    QFile f(updateDocumentPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
    QString code = doc.value("problem"_L1).toString();
    const QString detail = doc.value("detail"_L1).toString();
    enum { Tree, Binhost, Image } step = Tree;
    if (code.startsWith("binhost-"_L1)) {
        step = Binhost;
        code = code.mid(8);
    } else if (code.startsWith("image-"_L1)) {
        step = Image;
        code = code.mid(6);
    }
    if (step == Tree && code == "fetch-failed"_L1 && detail.contains("is older than this machine's"_L1))
        return {};
    QString origin;
    if (step == Binhost) {
        origin = doc.value("binhost_base"_L1).toString();
        if (origin.isEmpty())
            origin = doc.value("binhost"_L1).toString();
        if (origin.isEmpty())
            origin = i18nc("@info where the package index comes from, when no origin is recorded", "the binary host");
    } else {
        origin = doc.value("channel"_L1).toString();
        if (origin.isEmpty())
            origin = i18nc("@info where updates come from, when no origin is recorded", "the release's update server");
    }
    QString said;
    if (code == "no-network"_L1)
        said = step == Binhost ? i18nc("@info:status %1 is a URL or a directory", "The release's package index could not be fetched from %1: no network was found.", origin)
            : step == Image    ? i18nc("@info:status %1 is a URL or a directory", "The release's system image could not be fetched from %1: no network was found.", origin)
                               : i18nc("@info:status %1 is a URL or a directory", "Updates could not be fetched from %1: no network was found.", origin);
    else if (code == "fetch-failed"_L1)
        said = step == Binhost ? i18nc("@info:status %1 is a URL or a directory", "The release's package index could not be fetched from %1.", origin)
            : step == Image    ? i18nc("@info:status %1 is a URL or a directory", "The release's system image could not be fetched from %1.", origin)
                               : i18nc("@info:status %1 is a URL or a directory", "Updates could not be fetched from %1.", origin);
    else if (code == "no-key"_L1)
        said = i18nc("@info:status %1 is a URL or a directory",
                     "This computer holds no key that could verify a release from %1, so nothing was fetched.",
                     origin);
    else if (code == "unverified"_L1)
        said = step == Binhost ? i18nc("@info:status %1 is a URL or a directory",
                                       "The release's package index from %1 does not verify against this computer's keys.",
                                       origin)
            : step == Image ? i18nc("@info:status %1 is a URL or a directory",
                                    "The release's system image from %1 does not verify against this computer's keys.",
                                    origin)
                            : i18nc("@info:status %1 is a URL or a directory",
                                    "The last update installed nothing: what it fetched from %1 does not verify against this computer's keys.",
                                    origin);
    else if (code == "build-failed"_L1)
        said = i18nc("@info:status", "The last update installed nothing: building it from source failed.");
    else if (code == "needs-source"_L1)
        said = i18nc("@info:status", "The last update installed nothing: the release's package index does not cover it, and it needs building from source.");
    else if (step == Binhost && code == "absent"_L1)
        said = i18nc("@info:status %1 is a URL or a directory", "No package index is published at %1.", origin);
    if (said.isEmpty() || detail.isEmpty() || code == "no-network"_L1 || detail.contains(origin))
        return said;
    return i18nc("@info:status %1 is a sentence saying what failed, %2 is the update tool's own words", "%1 (%2)", said, detail);
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
    QSet<QString> removable; // `kdos update removable --json`: world ports nothing installed needs
    QHash<QString, QString> neededBy; // world ports an installed package needs, and one that does
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

// Pack icons and a binary host's embedded ones, written once per version: a
// new version is a new file name, so an icon changed by an update is never
// the old one from the cache.
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

static QHash<QString, QList<HostFace>> catalogueNames(const Catalogue &cat);
static QHash<QString, QString> catalogueByPackage(const Catalogue &cat);

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
    m_catalogueNames = catalogueNames(m_catalogue);
    m_catalogueByPackage = catalogueByPackage(m_catalogue);
    m_context.catalogue = &m_catalogue;
    m_context.mediumPacks = mediumPacksDir();
    m_context.live = rootIsLive();

    m_debianExclude = readDebianExclude();

    QFile pending(pendingPath());
    if (pending.open(QIODevice::ReadOnly)) {
        const Selection chosen = parseSelection(pending.readAll());
        m_chosenAtInstall = QSet<QString>(chosen.ids.cbegin(), chosen.ids.cend());
    }

    // A box created or removed in a terminal, a pack installed by
    // `kdos-appbox import`, a medium inserted, a `kdos update fetch` writing
    // the binhost index and its catalogue: each changes one of these
    // directories, and a settled change reads the machine again. The
    // binhost's directories do not exist on a fresh live medium until the
    // first fetch, so its parents are watched too and every change adds
    // whichever of them has appeared since.
    m_settle.setSingleShot(true);
    m_settle.setInterval(1500);
    connect(&m_settle, &QTimer::timeout, this, [this] {
        refresh();
    });
    const QString binhostXml = QFileInfo(binhostCatalogue()).absolutePath();
    const QStringList watched{QDir::home().filePath(u".config/kdos/boxes"_s),
                              packStoreDir(),
                              mediumPacksDir(),
                              u"/var/lib/kdos"_s,
                              QFileInfo(QFileInfo(binhostXml).absolutePath()).absolutePath(),
                              QFileInfo(binhostXml).absolutePath(),
                              binhostXml};
    const auto watch = [this, watched] {
        const QStringList have = m_watcher.directories();
        for (const QString &dir : watched) {
            if (!have.contains(dir) && QFileInfo(dir).isDir())
                m_watcher.addPath(dir);
        }
    };
    watch();
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this, watch] {
        watch();
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

// How much of an application page a component fills, compared as a tuple:
// a licence, then a developer or project group, then a description. A
// package can carry several components (paraview has paraview.desktop with
// no licence beside org.paraview.ParaView with one); the package keeps the
// richest, so a component with a licence wins wherever the pool lists it;
// of equals, the first the pool lists.
static std::tuple<bool, bool, bool> componentRichness(const AppStream::Component &c)
{
    return {!c.projectLicense().isEmpty(),
            !c.developer().name().isEmpty() || !c.projectGroup().isEmpty(),
            !c.description().isEmpty()};
}

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
            for (const QString &p : c.packageNames()) {
                const auto have = m_debianByPackage.constFind(p);
                if (have == m_debianByPackage.constEnd() || componentRichness(c) > componentRichness(*have))
                    m_debianByPackage.insert(p, c);
            }
        }
        qCDebug(LIBDISCOVER_BACKEND_KDOS_LOG) << "AppStream pool:" << all.size() << "components," << m_debianByPackage.size() << "Debian packages";
        m_poolReady = true;
        refresh();
    });
    m_pool->loadAsync();
}

// How a component id is compared across catalogues: without a trailing
// ".desktop". DEP-11 keeps the legacy ids (org.kde.kmail2.desktop) for
// what the host's metainfo calls org.kde.kmail2, and a twin compared by
// the bare string would be offered again in a box.
static QString hostKey(const QString &id)
{
    return id.endsWith(".desktop"_L1) ? id.chopped(8) : id;
}

// Every key a host component answers to: its id and each desktop entry it
// launches.
static void addHostKeys(QSet<QString> *keys, const AppStream::Component &c)
{
    keys->insert(hostKey(c.id()));
    for (const QString &entry : c.launchable(AppStream::Launchable::KindDesktopId).entries())
        keys->insert(hostKey(entry));
}

// Whether a Debian component is one the host catalogues already have, by
// its id or a desktop entry it launches. A program Debian lists under an
// id of its own (org.musescore.MuseScore3, schism.desktop) is caught by
// its name instead (hostTwinByName).
static bool isHostComponent(const QSet<QString> &keys, const AppStream::Component &c)
{
    if (keys.contains(hostKey(c.id())))
        return true;
    for (const QString &entry : c.launchable(AppStream::Launchable::KindDesktopId).entries()) {
        if (keys.contains(hostKey(entry)))
            return true;
    }
    return false;
}

// A package's stem: its leading run of ASCII letters, case-folded
// (musescore3 -> musescore, marble-qt -> marble).
static QString packageStem(const QString &pkg)
{
    qsizetype n = 0;
    while (n < pkg.size() && pkg.at(n).unicode() < 128 && pkg.at(n).isLetter())
        ++n;
    return pkg.left(n).toCaseFolded();
}

// A component's homepage as hostTwinByName compares it: the host,
// case-folded and without "www.". On a code-hosting site or an application
// directory the host names no program, so there it is the host and the
// first two segments of the path (github.com/flatpak/flatpak,
// apps.kde.org/kalk), and empty with no path at all: two programs called
// Notes that both live on github.com are not the same program.
static QString homepageHost(const QUrl &url)
{
    QString host = url.host().toCaseFolded();
    if (host.startsWith("www."_L1))
        host = host.mid(4);
    static const QSet<QString> forges = {
        u"github.com"_s, u"gitlab.com"_s, u"gitlab.gnome.org"_s, u"invent.kde.org"_s, u"apps.kde.org"_s,
        u"apps.gnome.org"_s, u"sourceforge.net"_s, u"codeberg.org"_s, u"gitlab.freedesktop.org"_s,
    };
    if (!forges.contains(host))
        return host;
    const QStringList segments = url.path().toCaseFolded().split(u'/', Qt::SkipEmptyParts);
    if (segments.isEmpty())
        return {};
    return host + u'/' + segments.mid(0, 2).join(u'/');
}

static QString homepageHost(const AppStream::Component &c)
{
    return homepageHost(c.url(AppStream::Component::UrlKindHomepage));
}

// Files a host desktop application under its case-folded name.
static void addHostName(QHash<QString, QList<HostFace>> *names, const AppStream::Component &c)
{
    const QString pkg = c.packageNames().value(0);
    if (c.kind() != AppStream::Component::KindDesktopApp || c.name().isEmpty() || pkg.isEmpty())
        return;
    names->operator[](c.name().toCaseFolded()).append({pkg, packageStem(pkg), homepageHost(c)});
}

// The package of the host desktop application a Debian component is the
// same program as, though no id is shared: the same name, case-folded, and
// either the same homepage (homepageHost) or package stems of four letters or more of
// which one begins the other (schism / schismtracker). A name alone is not
// enough: Calculator, Calendar and Mines name different programs in each
// archive, and those stay offered in a box. The cost: a Debian build of
// another major version of a port's program, such as MuseScore 3 beside the
// MuseScore port, is not offered while the port is. The name compared is
// Component::name(), the one in the session's language, so a pair of which
// only one catalogue translates the name is not caught and is offered
// twice. Empty when there is none.
static QString hostTwinByName(const QHash<QString, QList<HostFace>> &names, const AppStream::Component &c)
{
    const auto faces = names.constFind(c.name().toCaseFolded());
    if (c.name().isEmpty() || faces == names.constEnd())
        return {};
    const QString host = homepageHost(c);
    const QString stem = packageStem(c.packageNames().value(0));
    for (const HostFace &face : *faces) {
        if (!host.isEmpty() && host == face.homepage)
            return face.package;
        if (stem.size() >= 4 && face.stem.size() >= 4 && (stem.startsWith(face.stem) || face.stem.startsWith(stem)))
            return face.package;
    }
    return {};
}

// The catalogue's application rows as hostTwinByName compares a Debian
// component with them: under the row's case-folded name, a face for each of
// its packages, which names the row. A row packs its program with what the
// program needs (app.sonicpi is sonic-pi with supercollider, csound and
// puredata), and Debian lists the program by a package of its own beside
// those (supercollider-ide), which no row names: by its name and a package
// stem the row's share it is the row's program, and never a second
// resource of the same name in a box.
static QHash<QString, QList<HostFace>> catalogueNames(const Catalogue &cat)
{
    QHash<QString, QList<HostFace>> names;
    for (const CatalogueRow &row : cat.rows) {
        if (row.kind != "app"_L1 || row.name.isEmpty())
            continue;
        const QString host = homepageHost(QUrl(row.homepage));
        for (const QString &p : row.packages)
            names[row.name.toCaseFolded()].append({row.id, packageStem(p), host});
    }
    return names;
}

// Each package the catalogue's rows pack, naming the first row in the file
// that packs it.
static QHash<QString, QString> catalogueByPackage(const Catalogue &cat)
{
    QHash<QString, QString> rows;
    for (const CatalogueRow &row : cat.rows) {
        for (const QString &p : row.packages) {
            if (!rows.contains(p))
                rows.insert(p, row.id);
        }
    }
    return rows;
}

QString KdosBackend::catalogueRowOf(const AppStream::Component &c) const
{
    const QString row = m_catalogueByPackage.value(c.packageNames().value(0));
    if (!row.isEmpty())
        return row;
    if (c.kind() != AppStream::Component::KindDesktopApp)
        return {};
    return hostTwinByName(m_catalogueNames, c);
}

// Whether kdos-discover-catalog read a component from a bare desktop entry
// rather than a metainfo file. In a `marked` catalogue, one in which any
// component carries KDOS::BareEntry, the writer marked every such component
// KDOS::BareEntry=1 and nothing else, so the mark alone decides: a metainfo
// application with a .desktop id and no description is still metainfo. A
// licence decides nothing, since the writer gives every component its
// recipe's. A catalogue with no mark anywhere, as the published v0.2
// binhost's, which a fresh machine reads until its own release's catalogue
// is fetched, is read by what a desktop entry lacks: a desktop application
// whose id is a .desktop name and which has no description is a bare entry,
// since libappstream's desktop-entry reader writes none. Read by the mark
// alone, such a catalogue would list LibreOffice as Base, with Base's
// description.
static bool isBareEntry(const AppStream::Component &c, bool marked)
{
    if (marked)
        return c.customValue(u"KDOS::BareEntry"_s) == "1"_L1;
    return c.kind() == AppStream::Component::KindDesktopApp && c.id().endsWith(".desktop"_L1) && c.description().isEmpty();
}

// Whether a component is one of its package's own metainfo applications:
// not a bare entry, and described.
static bool isMetainfo(const AppStream::Component &c, bool marked)
{
    return !c.description().isEmpty() && !isBareEntry(c, marked);
}

// Which of a package's components it is listed by: a desktop application
// before a console one before anything else, then one with the package's
// own metainfo (isMetainfo), then one with an icon, then the shortest name.
// LibreOffice's package has seven applications, of which the start centre
// is only a desktop entry (isSuiteFace gives the suite its name back), and
// Okular's has its comic-book add-on beside Okular itself.
static bool listsBetter(const AppStream::Component &a, const AppStream::Component &b, bool marked)
{
    auto rank = [](const AppStream::Component &c) {
        switch (c.kind()) {
        case AppStream::Component::KindDesktopApp:
            return 2;
        case AppStream::Component::KindConsoleApp:
            return 1;
        default:
            return 0;
        }
    };
    if (rank(a) != rank(b))
        return rank(a) > rank(b);
    if (isMetainfo(a, marked) != isMetainfo(b, marked))
        return isMetainfo(a, marked);
    if (a.icons().isEmpty() != b.icons().isEmpty())
        return !a.icons().isEmpty();
    return a.name().size() < b.name().size();
}

// Whether a suite's package is listed by `face`, its shortest-named
// application, rather than `listed`, the metainfo application listsBetter
// picks: when the face is a bare desktop entry (isBareEntry) whose name
// begins the listed one's. The suite is then named, iconed and launched by
// its face and described as a suite (describeFace): LibreOffice's start
// centre, "LibreOffice", carries no description or developer, and Base,
// Calc and Draw, which do, would each stand for the whole suite under its
// own name. Any other face is left alone, so a package's minor tool never
// names the application beside it.
static bool isSuiteFace(const AppStream::Component &listed, const AppStream::Component &face, bool marked)
{
    return !face.id().isEmpty() && face.id() != listed.id() && isBareEntry(face, marked) && isMetainfo(listed, marked)
        && listed.name().startsWith(face.name() + u' ');
}

// One application of a suite as its description lists it: its name less
// the suite's ("Writer" for "LibreOffice Writer"), and its summary.
struct SuiteMember {
    QString name;
    QString summary;
};

// Gives `face` a description of the suite: a lead-in and each of `members`,
// the package's metainfo applications in catalogue order, by name and
// summary; one application's description would describe that application
// under the suite's name. The face's own summary stays out of the body:
// Discover shows a summary as the description's heading, and repeated as
// the body's first paragraph it would read twice. The licence, developer,
// categories and links are `listed`'s, the metainfo application
// listsBetter picked. `face` must be the only AppStream::Component holding
// its data: a setter on a shared one detaches it, the detached copy frees
// the GObject the other still holds, and the next read of that one crashes.
static void describeFace(AppStream::Component *face, const AppStream::Component &listed, const QList<SuiteMember> &members)
{
    QString body;
    if (!members.isEmpty()) {
        body = u"<p>"_s + i18nc("@info the line before the list of a suite's applications", "The suite holds:").toHtmlEscaped() + u"</p><ul>"_s;
        for (const SuiteMember &m : members) {
            QString name = m.name;
            if (name.startsWith(face->name() + u' '))
                name = name.mid(face->name().size() + 1);
            body += u"<li>"_s + (m.summary.isEmpty() ? name.toHtmlEscaped()
                                                     : i18nc("@info one application of a suite: %1 its name, %2 its summary", "%1: %2", name, m.summary).toHtmlEscaped())
                + u"</li>"_s;
        }
        body += u"</ul>"_s;
    }
    face->setDescription(body.isEmpty() ? listed.description() : body);
    face->setProjectLicense(listed.projectLicense());
    if (face->developer().name().isEmpty())
        face->setDeveloper(listed.developer());
    if (face->categories().isEmpty()) {
        for (const QString &category : listed.categories())
            face->addCategory(category);
    }
    for (const auto kind : {AppStream::Component::UrlKindHomepage,
                            AppStream::Component::UrlKindBugtracker,
                            AppStream::Component::UrlKindHelp,
                            AppStream::Component::UrlKindDonation}) {
        if (face->url(kind).isEmpty() && !listed.url(kind).isEmpty())
            face->addUrl(kind, listed.url(kind).toString());
    }
}

// What a search finds a package's resource by besides its own name: the
// names and keywords of all its components, one a line, carried on the
// listed component, so "writer" finds LibreOffice by its Writer.
static const auto SearchTermsKey = u"KDOS::SearchTerms"_s;

// The components of one catalogue file, one per package: the one listsBetter
// picks, the first in the file on a tie. Every component's keys go to `keys`.
// Every desktop application goes to `names` (addHostName).
// Read whole and outside the pool: a pool keys every OS catalogue component
// by the same data id, so a host port and its Debian twin (org.kde.kate is in
// both) would be one component, whichever section loaded last.
static QHash<QString, AppStream::Component>
readCatalogueFile(const QString &path, QSet<QString> *keys, QHash<QString, QList<HostFace>> *names)
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
    QHash<QString, QStringList> terms;
    QHash<QString, AppStream::Component> faces; // each package's shortest-named application
    QHash<QString, QList<SuiteMember>> members; // each package's metainfo applications, for describeFace
    const AppStream::ComponentBox box = md.components();
    bool marked = false;
    for (const AppStream::Component &c : box)
        marked |= !c.customValue(u"KDOS::BareEntry"_s).isEmpty();
    for (const AppStream::Component &c : box) {
        addHostKeys(keys, c);
        addHostName(names, c);
        const QString pkg = c.packageNames().value(0);
        if (pkg.isEmpty())
            continue;
        QStringList &t = terms[pkg];
        for (const QString &term : QStringList{c.name()} + c.keywords()) {
            if (!term.isEmpty() && !t.contains(term))
                t << term;
        }
        const auto have = out.constFind(pkg);
        if (have == out.constEnd() || listsBetter(c, *have, marked))
            out.insert(pkg, c);
        const auto face = faces.constFind(pkg);
        if (c.kind() == AppStream::Component::KindDesktopApp && (face == faces.constEnd() || c.name().size() < face->name().size()))
            faces.insert(pkg, c);
        if (c.kind() == AppStream::Component::KindDesktopApp && isMetainfo(c, marked))
            members[pkg].append({c.name(), c.summary()});
    }
    // The setters below run only on components nothing else holds (see
    // describeFace): the faces and the metainfo they replace are let go
    // first.
    QHash<QString, AppStream::Component> describers;
    for (auto it = out.begin(); it != out.end(); ++it) {
        const auto face = faces.constFind(it.key());
        if (face != faces.constEnd() && isSuiteFace(it.value(), *face, marked)) {
            describers.insert(it.key(), it.value());
            it.value() = *face;
        }
    }
    faces.clear();
    for (auto it = out.begin(); it != out.end(); ++it) {
        const auto describer = describers.constFind(it.key());
        if (describer != describers.constEnd())
            describeFace(&it.value(), *describer, members.value(it.key()));
        it->insertCustomValue(SearchTermsKey, terms.value(it.key()).join(u'\n'));
    }
    return out;
}

// The package of the host component that answers to `key`, read again from
// the catalogues: only an appstream:// link to a component no resource is
// listed by asks.
static QString hostPackageOf(const QString &key)
{
    QStringList files;
    for (const QString &root : swcatalogRoots())
        files << root + u"/xml/kdos-ports.xml"_s;
    files << binhostCatalogue();
    for (const QString &path : std::as_const(files)) {
        if (!QFileInfo::exists(path))
            continue;
        AppStream::Metadata md;
        md.setFormatStyle(AppStream::Metadata::FormatStyleCatalog);
        if (md.parseFile(path, AppStream::Metadata::FormatKindXml) != AppStream::Metadata::MetadataErrorNoError)
            continue;
        const AppStream::ComponentBox box = md.components();
        for (const AppStream::Component &c : box) {
            QSet<QString> keys;
            addHostKeys(&keys, c);
            if (keys.contains(key))
                return c.packageNames().value(0);
        }
    }
    return {};
}

// m_hostIds and m_hostNames hold the keys and names of both catalogues, the
// binhost's included: a program the release's binary host can add is
// offered as that port, never in a box.
void KdosBackend::readHostCatalogues()
{
    m_portsByPackage.clear();
    m_hostIds.clear();
    m_hostNames.clear();
    for (const QString &root : swcatalogRoots())
        m_portsByPackage.insert(readCatalogueFile(root + u"/xml/kdos-ports.xml"_s, &m_hostIds, &m_hostNames));
    m_binhostByPackage = readCatalogueFile(binhostCatalogue(), &m_hostIds, &m_hostNames);
}

// A desktop-entry category to the Breeze icon Discover's own categories use.
static QString categoryIcon(const QString &category)
{
    static const QHash<QString, QString> icons{
        {u"AudioVideo"_s, u"applications-multimedia"_s},
        {u"Development"_s, u"applications-development"_s},
        {u"Education"_s, u"applications-education"_s},
        {u"Game"_s, u"applications-games"_s},
        // KColorChooser's colour wheel, in breeze and breeze-dark alike,
        // reads as a graphics tool at list size. Breeze's
        // applications-graphics is a plain sphere and image-x-generic a
        // file-type placeholder: on a row of applications either reads as a
        // missing icon.
        {u"Graphics"_s, u"kcolorchooser"_s},
        {u"Network"_s, u"applications-internet"_s},
        {u"Office"_s, u"applications-office"_s},
        {u"Science"_s, u"applications-science"_s},
        {u"Settings"_s, u"preferences-system"_s},
        {u"System"_s, u"applications-system"_s},
        {u"Utility"_s, u"applications-utilities"_s},
    };
    return icons.value(category, u"applications-other"_s);
}

// The icon of a component that is no application and has no category of
// its own, by what kind of thing it is.
static QString kindIcon(AppStream::Component::Kind kind)
{
    switch (kind) {
    case AppStream::Component::KindFont:
        return u"preferences-desktop-font"_s;
    case AppStream::Component::KindCodec:
        return u"applications-multimedia"_s;
    case AppStream::Component::KindInputMethod:
        return u"input-keyboard"_s;
    case AppStream::Component::KindLocalization:
        return u"preferences-desktop-locale"_s;
    case AppStream::Component::KindDriver:
    case AppStream::Component::KindFirmware:
        return u"hwinfo"_s;
    case AppStream::Component::KindService:
        return u"preferences-system-services"_s;
    default:
        return u"application-x-sharedlib"_s;
    }
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

// A stock icon's file in hicolor or pixmaps. The icon theme a process has
// loaded does not see icons installed after it read the theme, so a package
// Discover has just installed finds its icon here until Discover restarts.
static QString hicolorFile(const QString &name)
{
    static const QStringList sizes{u"scalable"_s, u"256x256"_s, u"128x128"_s, u"64x64"_s, u"48x48"_s, u"32x32"_s, u"24x24"_s, u"16x16"_s};
    const QStringList roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &root : roots) {
        for (const QString &size : sizes) {
            for (const auto *ext : {".svg", ".png"}) {
                const QString path = root + u"/icons/hicolor/"_s + size + u"/apps/"_s + name + QLatin1String(ext);
                if (QFileInfo::exists(path))
                    return path;
            }
        }
        for (const auto *ext : {".svg", ".png"}) {
            const QString path = root + u"/pixmaps/"_s + name + QLatin1String(ext);
            if (QFileInfo::exists(path))
                return path;
        }
    }
    return {};
}

// The first stored icon file the component names, else its stock name when
// the icon theme has it, else that name's file in hicolor: Debian's icons
// are not installed on the host, and a name the theme lacks is drawn as
// Discover's missing-icon fallback.
static QString iconOf(const AppStream::Component &c)
{
    QString stock, file;
    const auto icons = c.icons();
    for (const AppStream::Icon &icon : icons) {
        if (icon.kind() == AppStream::Icon::KindCached || icon.kind() == AppStream::Icon::KindLocal) {
            const QString path = icon.url().toLocalFile();
            if (!path.isEmpty() && QFileInfo::exists(path))
                return path;
        } else if (icon.kind() == AppStream::Icon::KindStock && stock.isEmpty()) {
            if (QIcon::hasThemeIcon(icon.name()))
                stock = icon.name();
            else if (file.isEmpty())
                file = hicolorFile(icon.name());
        }
    }
    return stock.isEmpty() ? file : stock;
}

// The launcher's Icon= is the icon the Start menu draws the application by:
// a file, a name the icon theme has, or the name of a file genlaunchers
// exported into the user's hicolor, which a theme read before the export
// does not see (hicolorFile). It replaces only a stand-in, the icon of a
// category or a kind: a pack's own icon or a component's stays.
void KdosBackend::applyLauncher(Entry *e, bool iconIsStandIn) const
{
    if (e->box.isEmpty())
        return;
    const auto l = m_launchers.constFind(e->box);
    if (l == m_launchers.constEnd())
        return;
    e->launcher = l->path;
    if (!iconIsStandIn || l->icon.isEmpty())
        return;
    if (QDir::isAbsolutePath(l->icon)) {
        if (QFileInfo::exists(l->icon))
            e->icon = l->icon;
    } else if (QIcon::hasThemeIcon(l->icon)) {
        e->icon = l->icon;
    } else if (const QString file = hicolorFile(l->icon); !file.isEmpty()) {
        e->icon = file;
    }
}

void KdosBackend::enrichFromComponent(Entry *e, const AppStream::Component &c) const
{
    if (e->summary.isEmpty())
        e->summary = c.summary();
    if (e->description.isEmpty() && !c.description().isEmpty()) {
        e->description = c.description();
        e->descriptionIsMarkup = true;
    }
    QString icon = iconOf(c);
    // A binary host's component carries its icon file (KDOS::IconData,
    // kdos-discover-catalog's embedIcon), since the machine has not
    // installed the package whose theme icon its stock name is; it is
    // written to the icon cache once per component and version.
    if (icon.isEmpty()) {
        const QString data = c.customValue(u"KDOS::IconData"_s);
        if (!data.isEmpty()) {
            const QString key = u"binhost-"_s + c.id();
            const QString version = c.customValue(u"KDOS::Version"_s);
            icon = cachedIcon(key, version);
            if (icon.isEmpty())
                icon = storeIcon(key, version, QByteArray::fromBase64(data.toLatin1()));
        }
    }
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
    // The developer's name, else the project group DEP-11 files the
    // component under (KStars and Okteta name no developer, only "KDE"):
    // an empty author is shown as "Unknown author".
    if (e->author.isEmpty())
        e->author = c.developer().name();
    if (e->author.isEmpty())
        e->author = c.projectGroup();
    // The entry launched is the first one installed: a metainfo may name an
    // entry its package does not install before the one the catalogue
    // added that it does (kdos-discover-catalog).
    const QStringList entries = c.launchable(AppStream::Launchable::KindDesktopId).entries();
    e->desktopIds = entries;
    for (const QString &entry : entries) {
        if (!QStandardPaths::locate(QStandardPaths::ApplicationsLocation, entry).isEmpty()) {
            e->desktopId = entry;
            break;
        }
    }
    if (e->desktopId.isEmpty() && !entries.isEmpty())
        e->desktopId = entries.first();
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
    const bool standIn = e.icon.isEmpty();
    if (standIn)
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
        applyLauncher(&e, standIn);
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

// A search's Debian component is one every word of the query names: each
// word, case-folded, occurs in the component's name, id, package name or one
// of its keywords. AppStream's own search also matches summaries and
// descriptions, and over a catalogue of thousands that answers almost any
// query, arithmetic included, with applications it does not name; those
// would also fill DebianQueryLimit before the ones it does. Not static:
// backendtest declares and calls it.
bool debianMatches(const AppStream::Component &c, const QStringList &words);
bool debianMatches(const AppStream::Component &c, const QStringList &words)
{
    QStringList fields{c.name().toCaseFolded(), c.id().toCaseFolded()};
    for (const QString &p : c.packageNames())
        fields << p.toCaseFolded();
    for (const QString &k : c.keywords())
        fields << k.toCaseFolded();
    return !words.isEmpty() && std::all_of(words.cbegin(), words.cend(), [&fields](const QString &w) {
        return std::any_of(fields.cbegin(), fields.cend(), [&w](const QString &f) {
            return f.contains(w);
        });
    });
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

    const AppStream::ComponentBox box = filter.search.isEmpty() ? m_pool->componentsByCategories(filter.category->involvedCategories())
                                                                : m_pool->search(filter.search);
    const QStringList words = filter.search.toCaseFolded().split(u' ', Qt::SkipEmptyParts);
    for (const AppStream::Component &c : box) {
        if (out.size() >= DebianQueryLimit)
            break;
        if (!c.origin().startsWith("debian"_L1) || c.kind() != AppStream::Component::KindDesktopApp)
            continue;
        if (!filter.search.isEmpty() && !debianMatches(c, words))
            continue;
        // A program the host has natively is never offered again in a box,
        // whether Debian lists it by the host's id or under the same name
        // (isHostComponent, hostTwinByName), nor one a catalogue row is
        // (catalogueRowOf).
        const QString pkg = c.packageNames().value(0);
        if (!isDebianPackage(pkg) || !catalogueRowOf(c).isEmpty() || isHostComponent(m_hostIds, c)
            || !hostTwinByName(m_hostNames, c).isEmpty() || m_debianExclude.contains(pkg))
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

    snap->outstanding = 7;
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
        // kpkg answered only when it exited 0 and printed a package array:
        // an empty or garbled answer taken for "nothing installed" would
        // drop every image port off the installed list.
        snap->portsOk = ok && QJsonDocument::fromJson(out).object().value(u"packages"_s).isArray();
        landed();
    });
    // Which installed ports a person may remove: a world port nothing
    // installed depends on. An answer that did not come leaves every port
    // part of the system, which offers no Remove that would refuse.
    runTool(this, u"kdos"_s, {u"update"_s, u"removable"_s, u"--json"_s}, [snap, landed](const QByteArray &out, bool ok) {
        const QJsonObject doc = ok ? QJsonDocument::fromJson(out).object() : QJsonObject();
        const QJsonArray removable = doc.value("removable"_L1).toArray();
        for (const QJsonValue &v : removable)
            if (isSafeId(v.toString()))
                snap->removable.insert(v.toString());
        const QJsonObject needed = doc.value("needed"_L1).toObject();
        for (auto it = needed.constBegin(); it != needed.constEnd(); ++it)
            if (isSafeId(it.key()) && isSafeId(it.value().toString()))
                snap->neededBy.insert(it.key(), it.value().toString());
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
    m_launchers = boxLaunchers();
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
        e.commands = row.commands;
        e.size = l.bytes;
        e.sizeIsEstimate = true;
        e.icon = categoryIcon(l.category);
        // A row's own `about` comes first; a Debian component below fills
        // only what it leaves empty.
        e.vendorDeb = row.vendorDeb;
        e.license = row.license;
        e.author = row.author;
        e.homepage = row.homepage;
        e.description = row.description;

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
        // A data row reaches its program only as a pack, which kdos-packd
        // grafts where the program looks; built by the store it is an image
        // nothing reads, so one with no pack is offered only to be removed.
        if (e.kind == "data"_L1 && e.packOrigin.isEmpty() && !l.installed)
            continue;
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
        applyLauncher(&e, !snap.packIcons.contains(l.id) && e.icon == categoryIcon(l.category));
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
        // KDOS gathers a set; its applications are each their own
        // publisher's, and the set is under every licence its applications
        // read above name. One whose licence nobody gives adds none.
        e.author = i18nc("@info the author of a catalogue set", "KDOS");
        QStringList licences;
        for (const QString &mid : g.members) {
            const KdosResource *member = m_resources.value(u"catalogue/"_s + mid);
            const QString l = member ? member->entry().license : QString();
            if (!l.isEmpty() && !licences.contains(l))
                licences << l;
        }
        e.license = licences.size() > 1 ? u"("_s + licences.join(u") AND ("_s) + u')' : licences.value(0);
        for (const QString &mid : g.members) {
            if (m_catalogue.rows.value(mid).vendorDeb)
                e.vendorDeb = true;
        }
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
            applyLauncher(&e, !snap.packIcons.contains(id));
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
        applyLauncher(&e, true);
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
    // kpkg is the authority on what is installed; the image's catalogue only
    // describes it. A package of that catalogue is installed when kpkg lists
    // it, or when kpkg did not answer, by the catalogue's own version; one
    // kpkg answered is absent is not listed from it, since `kpkg remove`
    // leaves the catalogue as it was and Discover would offer to launch what
    // is gone. What a binhost could add comes from its catalogue, only for a
    // package kpkg answered is absent, so a kpkg that did not answer offers
    // nothing. Both catalogues are read again here: kdos-ports-helper
    // rewrites the first after every install, and `kdos update fetch`
    // replaces the second.
    readHostCatalogues();
    {
        auto portEntry = [this, &snap](const QString &pkg, const AppStream::Component &c) {
            Entry e;
            e.lane = Lane::Port;
            e.id = pkg;
            e.removable = snap.removable.contains(pkg);
            e.neededBy = snap.neededBy.value(pkg);
            e.name = c.name().isEmpty() ? pkg : c.name();
            e.appstreamId = c.id();
            e.kind = AppStream::Component::kindToString(c.kind());
            enrichFromComponent(&e, c);
            // A component that names no icon the theme has: a terminal for a
            // console program, a plugin for an add-on, its category's for an
            // application, and for anything else its category's when it has
            // one, else one of its kind (libfprint and pcscd are generic
            // components with no category).
            if (e.icon.isEmpty()) {
                const QString byCategory = categoryIcon(e.categories);
                switch (c.kind()) {
                case AppStream::Component::KindConsoleApp:
                    e.icon = u"utilities-terminal"_s;
                    break;
                case AppStream::Component::KindAddon:
                    e.icon = u"preferences-plugin"_s;
                    break;
                case AppStream::Component::KindDesktopApp:
                case AppStream::Component::KindWebApp:
                    e.icon = byCategory;
                    break;
                default:
                    e.icon = byCategory != "applications-other"_L1 ? byCategory : kindIcon(c.kind());
                    break;
                }
            }
            // Discover's All Applications page lists a resource by its
            // categories, so an application whose metainfo and desktop entry
            // name none is filed under Other rather than left off it.
            if (e.categories.isEmpty() && c.kind() == AppStream::Component::KindDesktopApp)
                e.categories << u"Other"_s;
            return e;
        };
        auto searchTerms = [](const AppStream::Component &c) {
            return c.customValue(SearchTermsKey).split(u'\n', Qt::SkipEmptyParts);
        };
        for (auto it = m_portsByPackage.cbegin(); it != m_portsByPackage.cend(); ++it) {
            if (!isSafeId(it.key()))
                continue;
            const QString version = snap.ports.value(it.key());
            if (snap.portsOk && version.isEmpty())
                continue;
            Entry e = portEntry(it.key(), *it);
            e.installedVersion = version.isEmpty() ? it->customValue(u"KDOS::Version"_s) : version;
            e.availableVersion = e.installedVersion;
            e.state = EntryState::Installed;
            upsert(e, &seen)->setSearchTerms(searchTerms(*it));
        }
        if (snap.portsOk) {
            for (auto it = m_binhostByPackage.cbegin(); it != m_binhostByPackage.cend(); ++it) {
                if (!isSafeId(it.key()))
                    continue;
                Entry e = portEntry(it.key(), *it);
                const QString version = snap.ports.value(it.key());
                e.availableVersion = it->customValue(u"KDOS::Version"_s);
                e.installedVersion = version;
                e.state = version.isEmpty() ? EntryState::None : EntryState::Installed;
                // An installed port the image's catalogue lists is that
                // catalogue's resource, read above; the twin takes its state.
                const KdosResource *listed = m_resources.value(u"port/"_s + it.key());
                if (m_portsByPackage.contains(it.key()) && !version.isEmpty() && listed) {
                    e.state = listed->entry().state;
                    e.installedVersion = listed->entry().installedVersion;
                    e.availableVersion = listed->entry().availableVersion;
                    e.removable = listed->entry().removable;
                    e.neededBy = listed->entry().neededBy;
                } else {
                    upsert(e, &seen)->setSearchTerms(searchTerms(*it));
                }
                // The same port, built here: a second origin in Discover's
                // source picker, kept whether or not the port is installed.
                // Discover's application page keeps the most sources it has
                // seen while it is open, so a twin that came and went with
                // the install would leave the page with one source after a
                // removal: no picker, and an "Install from" button.
                e.lane = Lane::PortSource;
                upsert(e, &seen)->setSearchTerms(searchTerms(*it));
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
    // Said again only when it changes: Discover keeps one message for every
    // backend, and posting the same one again would flicker it.
    static const char ProblemShown[] = "kdosUpdateProblem";
    const QString problem = updateProblemText();
    if (problem != property(ProblemShown).toString()) {
        setProperty(ProblemShown, problem);
        Q_EMIT inlineMessageChanged(problem.isEmpty() ? QSharedPointer<InlineMessage>()
                                                      : QSharedPointer<InlineMessage>::create(InlineMessage::Warning, u"dialog-warning"_s, problem));
    }
    Q_EMIT contentsChanged();
    Q_EMIT updatesCountChanged();
}

// ── search ──────────────────────────────────────────────────────────────

// A search term, read once per search. `categories` are the desktop-entry
// categories the word stands for: "browser" is a word for what a web browser
// is, and the applications that are one carry WebBrowser, whatever their names.
// `kin` matches, as a whole word or its plural, any word that stands for the
// same categories ("mail", "email" and "e-mail" are one), in a summary.
// `content` is the part of `kin` that names what a program of the category
// handles (mail, chat, music, games), set only for those categories; a word
// that also qualifies other programs ("web" interface, "video" settings,
// graphics "editor", run in a "terminal") is not in it. `parent` is the
// category the term's categories belong under (AudioVideo for Video, Music
// and Audio), which an application may file alone with a narrower one the
// term does not name (AudioVideoEditing, Player, Recorder).
struct SearchQuery {
    QString term;
    QRegularExpression word; // the term between non-word characters
    QStringList categories;
    QString parent;
    QRegularExpression kin;
    QRegularExpression content;
};

// The head of a summary, the words before its first "for", "with" or "to":
// past them the noun names what the program serves or carries, not what it
// is.
static QStringView summaryHead(const QString &summary)
{
    static const QRegularExpression tail(u"(?<!\\w)(?:for|with|to)(?!\\w)"_s,
                                         QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpressionMatch m = tail.match(summary);
    return m.hasMatch() ? QStringView(summary).left(m.capturedStart()) : QStringView(summary);
}

// Whether a summary calls its program an accessory of its category, by its
// head: Ymuse, "GTK client for Music Player Daemon", is a music player, while
// Birdtray, "System tray new mail notification for Thunderbird", is a tray
// icon by the words before its "for", and FFmpeg Thumbnailer, "Video
// thumbnail generator", serves a file manager.
static bool isAccessory(const QString &summary)
{
    static const QRegularExpression nouns(u"(?<![\\w-])(?:notification|notifier|notify|tray|daemon|applet|indicator|gateway|plug-?in|thumbnailer|thumbnail\\s+generator)s?(?![\\w-])"_s,
                                          QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return nouns.matchView(summaryHead(summary)).hasMatch();
}

// Whether a resource is media plumbing for a term whose categories belong
// under AudioVideo (SearchQuery::parent: "video" and "music"): it files
// itself under both Audio and Video, which names no medium, and the head of
// its summary says neither the term's word (SearchQuery::kin) nor media or
// player. qpwgraph, "A PipeWire Graph Qt GUI Interface", routes streams and
// plays none; mpv and SMPlayer, "media player", play them. Any other term is
// ranked as if the test did not exist.
static bool isMediaPlumbing(const Entry &e, const QString &summary, const QString &parent, const QRegularExpression &kin)
{
    static const QRegularExpression media(u"(?<![\\w-])(?:media|multimedia|player)s?(?![\\w-])"_s,
                                          QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    if (parent != "AudioVideo"_L1 || !e.categories.contains("Audio"_L1) || !e.categories.contains("Video"_L1))
        return false;
    const QStringView head = summaryHead(summary);
    return !kin.matchView(head).hasMatch() && !media.matchView(head).hasMatch();
}

static SearchQuery searchQuery(const QString &term)
{
    static const QHash<QString, QStringList> aliases = {
        {u"browser"_s, {u"WebBrowser"_s}},
        {u"web"_s, {u"WebBrowser"_s}},
        {u"mail"_s, {u"Email"_s}},
        {u"email"_s, {u"Email"_s}},
        {u"e-mail"_s, {u"Email"_s}},
        {u"office"_s, {u"Office"_s}},
        {u"terminal"_s, {u"TerminalEmulator"_s}},
        {u"editor"_s, {u"TextEditor"_s}},
        {u"chat"_s, {u"InstantMessaging"_s, u"Chat"_s}},
        {u"music"_s, {u"Music"_s, u"Audio"_s}},
        {u"video"_s, {u"Video"_s}},
        {u"game"_s, {u"Game"_s}},
        {u"games"_s, {u"Game"_s}},
    };
    static const QStringList contentWords = {u"mail"_s, u"email"_s, u"e-mail"_s, u"chat"_s, u"music"_s, u"game"_s, u"games"_s};
    const auto opts = QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption;
    SearchQuery q;
    q.term = term.trimmed();
    q.word = QRegularExpression(u"(?<!\\w)"_s + QRegularExpression::escape(q.term) + u"(?!\\w)"_s, opts);
    q.categories = aliases.value(q.term.toLower());
    if (q.categories.contains("Video"_L1) || q.categories.contains("Audio"_L1))
        q.parent = u"AudioVideo"_s;
    if (!q.categories.isEmpty()) {
        QStringList kin, content;
        for (auto it = aliases.cbegin(); it != aliases.cend(); ++it) {
            if (it.value() != q.categories)
                continue;
            kin << QRegularExpression::escape(it.key());
            if (contentWords.contains(it.key()))
                content << QRegularExpression::escape(it.key());
        }
        q.kin = QRegularExpression(u"(?<![\\w-])(?:"_s + kin.join(u'|') + u")s?(?![\\w-])"_s, opts);
        if (!content.isEmpty())
            q.content = QRegularExpression(u"(?<![\\w-])(?:"_s + content.join(u'|') + u")s?(?![\\w-])"_s, opts);
    }
    return q;
}

// A search's ranking, highest first. A resource takes the best of the ranks
// it earns:
//   100  its name is the term
//    85  a keyword or component name is the whole term, for a term that
//        names no category
//   75+  it is an application of the category the term names; +3 when its
//        summary says a word for that category, +2 when its name holds the
//        term, so "mail" lists KMail and Thunderbird ahead of the Email
//        tools that only file themselves there (Account Wizard, KTnef)
//    74  a keyword is the whole term the category names, for a program
//        outside the category
//    70  its name starts with the term; 60 holds it as a word; 50 anywhere
//    40  its package name holds it; 30 a keyword does; 20 its summary does
// So "browser" lists the web browsers before AT-SPI Browser, and "chrome"
// lists Chromium, whose keyword it is, before mkchromecast. For a term that
// names a category, membership decides and a keyword does not: "video" lists
// the video players and editors ahead of Qt V4L2 test Utility, AudioVideo
// only and keyword video, which still lists above a name that starts with
// the term. A catalogue row or a pack names one main category (Network, not
// WebBrowser), so for those a summary that says a word for the category
// stands in for the category and earns the full 75+: Surf, "Simple web
// browser", is a web browser. An application that files itself under the
// term's parent category (SearchQuery::parent, AudioVideo) with a narrower
// one the term does not name is in the category when the head of its summary
// (summaryHead()) says a word for it: Kdenlive, AudioVideoEditing, "Video
// editor", is a video application. Any other resource that does not file
// itself there takes a flat 75 when the head of its summary says a word for
// what the category's programs handle (SearchQuery::content: mail, chat,
// music, games): with the category's members that do not say it, below the
// ones that both file themselves there and say it, above every name, keyword
// or summary hit. Kontact, Office only, "Handle all your emails", lists with
// the mail applications, ahead of KMail Import Wizard (its name) and gLabels
// (its "mail merge" keyword); SieveEditor, "Editor for email filter Sieve
// scripts", does not. The categories whose words also qualify other programs
// take no content stand-in, so "browser" does not list CUPS, "CUPS Web
// Interface", and "video" does not list Chocolate Doom Setup, "Controls,
// sound, video and network settings". A Debian build in a box takes the flat
// 75 for the category and both stand-ins, with no +3 or +2, so every native
// application of the category that says the word lists above it. A program
// that runs only in a terminal (ConsoleOnly, or a console-application
// component) takes 65 for the keyword and category ranks, below every name
// match, so "mail" lists the mail applications that draw a window ahead of
// aerc, and a terminal program is still first for its own name.
// kdos-discover-catalog adds ConsoleOnly to a Terminal=true entry. An
// accessory, a program whose summary calls it a notifier, tray icon, daemon,
// applet, indicator, gateway, plugin, thumbnailer or thumbnail generator
// (isAccessory(): a client for a daemon is not one), takes at most 72 for
// the keyword and category ranks: below every application of the category,
// above a name that starts with the term, so "mail" lists Evolution, Geary
// and Thunderbird ahead of Mailnag and Birdtray, and an accessory is still
// first for its own name. Media plumbing takes the same cap for "video" and
// "music", the terms whose categories belong under AudioVideo: an
// application filed under both Audio and Video names no medium, so the
// head of its summary has to say the term's word, media or player
// (isMediaPlumbing()). "video" and "music" list qpwgraph, "A PipeWire Graph
// Qt GUI Interface", after every video and music application, Debian builds
// included; mpv and SMPlayer, "media player", stay with the players.
//
// Discover orders a search by its SearchRelevanceRole, sortScore / 100 plus
// the rating, up to 10 for the name word nearest the term and 5 or 10 when
// the name holds or is the term (ResourcesProxyModel.cpp). search() emits
// the rank times RankScale, so the smallest step between two ranks (1 point,
// 77 to 78, worth 100 after the division) outweighs the at most 20 the name
// terms add (no KDOS source has a rating), and Discover's terms only order
// resources of one rank. Every result but a Debian package's (a port, a
// catalogue row or a pack) adds RankScale / 2 (50 after the division):
// above the name terms and below a rank step, so within one rank the binary
// host's ports and packs list before a Debian build. At a scale of 1 a rank
// is worth at most a point, and AT-SPI Browser's name beats Chromium's
// keyword.
static constexpr uint RankScale = 10000;

static uint matchScore(KdosResource *res, const SearchQuery &q)
{
    const QString &term = q.term;
    if (term.isEmpty())
        return 1;
    const QString name = res->name();
    if (name.compare(term, Qt::CaseInsensitive) == 0)
        return 100;
    const Entry &e = res->entry();
    const bool console = e.categories.contains("ConsoleOnly"_L1) || e.kind == "console-application"_L1;
    const bool accessory = isAccessory(res->comment()) || isMediaPlumbing(e, res->comment(), q.parent, q.kin);
    const uint cap = console ? 65 : accessory ? 72 : 100;
    const bool nameHolds = name.contains(term, Qt::CaseInsensitive);
    uint score = 0;
    auto earn = [&score](uint s) {
        score = std::max(score, s);
    };
    const bool keyword = std::any_of(res->searchTerms().cbegin(), res->searchTerms().cend(), [&term](const QString &t) {
        return t.compare(term, Qt::CaseInsensitive) == 0;
    });
    if (q.categories.isEmpty()) {
        if (keyword)
            earn(std::min(cap, 85u));
    } else {
        const QString summary = res->comment();
        const bool summarySays = q.kin.match(summary).hasMatch();
        const bool mainOnly = e.lane == Lane::Catalogue || e.lane == Lane::Pack;
        const bool debian = e.lane == Lane::Debian;
        bool inCategory = mainOnly && summarySays;
        for (const QString &c : q.categories)
            inCategory |= e.categories.contains(c);
        if (!inCategory && !q.parent.isEmpty() && e.categories.contains(q.parent))
            inCategory = q.kin.matchView(summaryHead(summary)).hasMatch();
        if (inCategory)
            earn(std::min(cap, 75u + (debian ? 0 : (summarySays ? 3 : 0) + (nameHolds ? 2 : 0))));
        else if (!q.content.pattern().isEmpty() && q.content.matchView(summaryHead(summary)).hasMatch())
            earn(std::min(cap, 75u));
        else if (keyword)
            earn(std::min(cap, 74u));
    }
    if (name.startsWith(term, Qt::CaseInsensitive))
        earn(70);
    else if (q.word.match(name).hasMatch())
        earn(60);
    else if (nameHolds)
        earn(50);
    if (score)
        return score;
    if (res->packageName().contains(term, Qt::CaseInsensitive))
        return 40;
    for (const QString &t : res->searchTerms()) {
        if (t.contains(term, Qt::CaseInsensitive))
            return 30;
    }
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
        const SearchQuery query = searchQuery(filter.search);
        QVector<StreamResult> found;
        QSet<KdosResource *> have;
        const bool source = preferSource();
        auto consider = [&](KdosResource *res) {
            if (have.contains(res))
                return;
            const uint score = matchScore(res, query);
            if (score == 0 || !filter.shouldFilter(res))
                return;
            have.insert(res);
            found << StreamResult{res, score * RankScale + (res->entry().lane == Lane::Debian ? 0 : RankScale / 2)};
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
        // A component of a host package the package is not listed by, such
        // as LibreOffice Writer of the libreoffice port: that port.
        if (found.isEmpty()) {
            for (const QString &id : ids) {
                if (!m_hostIds.contains(hostKey(id)))
                    continue;
                const QString pkg = hostPackageOf(hostKey(id));
                for (const QString &lane : {u"port/"_s, u"port-source/"_s}) {
                    if (auto *res = m_resources.value(lane + pkg); res && !found.contains(StreamResult{res, 0}))
                        found << StreamResult{res, 0};
                }
            }
        }
        // An appstream:// link to an application only Debian's DEP-11 knows,
        // as KRunner's software results give: the catalogue row it is
        // (catalogueRowOf) when there is one, else the Debian lane's resource.
        // One the host has under the same name (hostTwinByName) is that
        // host port, as debianFor never lists it.
        if (found.isEmpty() && m_poolReady) {
            for (const QString &id : ids) {
                if (m_hostIds.contains(hostKey(id)))
                    continue;
                const AppStream::ComponentBox box = m_pool->componentsById(id);
                for (const AppStream::Component &c : box) {
                    const QString pkg = c.packageNames().value(0);
                    if (!c.origin().startsWith("debian"_L1) || !isDebianPackage(pkg))
                        continue;
                    if (c.kind() == AppStream::Component::KindDesktopApp) {
                        if (const QString hostPkg = hostTwinByName(m_hostNames, c); !hostPkg.isEmpty()) {
                            for (const QString &lane : {u"port/"_s, u"port-source/"_s}) {
                                if (auto *res = m_resources.value(lane + hostPkg); res && !found.contains(StreamResult{res, 0}))
                                    found << StreamResult{res, 0};
                            }
                            continue;
                        }
                    }
                    const QString row = catalogueRowOf(c);
                    KdosResource *res = row.isEmpty() ? nullptr : m_resources.value(u"catalogue/"_s + row);
                    if (row.isEmpty() && c.kind() == AppStream::Component::KindDesktopApp && !m_debianExclude.contains(pkg))
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

// A port the binhost holds is two resources with one AppStream id, the
// binhost's and the one built here, installed or not. Discover's lists keep
// one resource per id, and which of two equal rows survives its sort no
// backend decides; the application page then offers the rest in its source
// picker, which it fills by asking for the application's URL, and a link
// opens the first resource that answers. So a list names only the twin
// `install =` prefers, the binhost's once the port is installed, and a URL
// answers both, that one first: the other is the second twin.
bool KdosBackend::isSecondTwin(const KdosResource *res, bool preferSource) const
{
    const Entry &e = res->entry();
    if (e.lane == Lane::PortSource) {
        if (!preferSource)
            return true;
        const KdosResource *port = m_resources.value(u"port/"_s + e.id);
        return port && port->entry().state != EntryState::None;
    }
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
