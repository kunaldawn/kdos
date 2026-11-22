/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — KdosBackend against a machine made of fixtures
 *
 * The KDOS tools are shell scripts on PATH that print the fixtures and write
 * every argument vector they are given to a log; kdos-packd is a local
 * socket served from this process; the catalogues are fixture catalogue
 * roots. What is asserted is what Discover would show and what an install
 * or a removal would run, through the backend's own Discover-facing API.
 * ---------------------------------
 */

#include "KdosBackend.h"
#include "KdosResource.h"
#include "KdosTransaction.h"

#include <Category/Category.h>
#include <Transaction/Transaction.h>
#include <Transaction/TransactionModel.h>

#include <AppStreamQt/component.h>

#include <KTar>

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkInformation>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopeGuard>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace Qt::StringLiterals;
using namespace Kdos;

using Filters = AbstractResourcesBackend::Filters;

namespace Kdos
{
bool debianMatches(const AppStream::Component &c, const QStringList &words);
}

class BackendTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void catalogueRows();
    void sets();
    void packsAndBoxes();
    void debianLane();
    void hostPorts();
    void hostCatalogueListing();
    void unmarkedSuiteListing();
    void pendingAndCategories();
    void archiveFile();
    void packFile();
    void installMetered();
    void installFails();
    void removeRuns();
    void offlineRefuses();
    void debianNameMatch();
    void refusals();
    void urlBeforeFirstRefresh();
    void boxesCategoryNeedsABox();
    void failedCheckIsSaid();
    void portRemovedOnRequest();
    void cancelClosesHelperStdin_data();
    void cancelClosesHelperStdin();
    void shippedExclusions();
    void searchRanking();

private:
    QList<KdosResource *> find(const Filters &filter);
    KdosResource *resource(const QString &lane, const QString &id);
    void writeTool(const QString &name, const QByteArray &body);
    QStringList toolLog();
    void clearLog();
    Transaction::Status runToEnd(Transaction *t, QList<int> *progress = nullptr);
    QByteArray fixture(const QString &name);

    QTemporaryDir m_dir;
    QString m_fixtures;
    QLocalServer m_packd;
    QStringList m_packdRequests;
    QStringList m_asked; // the last transaction's proceed question: title, text
    bool m_answer = true;
    KdosBackend *m_backend = nullptr;
};

QByteArray BackendTest::fixture(const QString &name)
{
    QFile f(m_fixtures + u'/' + name);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", qPrintable(name));
    return f.readAll();
}

void BackendTest::writeTool(const QString &name, const QByteArray &body)
{
    const QString path = m_dir.filePath(u"bin/"_s + name);
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("#!/bin/sh\nprintf '%s\\n' \"" + name.toUtf8() + " $*\" >> \"$KDOS_TEST_LOG\"\n" + body + "\nexit 0\n");
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
}

QStringList BackendTest::toolLog()
{
    QFile f(m_dir.filePath(u"tools.log"_s));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll()).split(u'\n', Qt::SkipEmptyParts);
}

void BackendTest::clearLog()
{
    QFile::remove(m_dir.filePath(u"tools.log"_s));
    m_packdRequests.clear();
}

QList<KdosResource *> BackendTest::find(const Filters &filter)
{
    QList<KdosResource *> out;
    QPointer<ResultsStream> stream = m_backend->search(filter);
    connect(stream, &ResultsStream::resourcesFound, this, [&out](const QVector<StreamResult> &found) {
        for (const StreamResult &r : found)
            out << qobject_cast<KdosResource *>(r.resource);
    });
    for (int i = 0; stream && i < 500; i++)
        QTest::qWait(20);
    if (stream)
        qWarning() << "a search never finished" << filter;
    return out;
}

KdosResource *BackendTest::resource(const QString &lane, const QString &id)
{
    Filters f;
    f.resourceUrl = QUrl(u"kdos-backend://"_s + lane + u'/' + id);
    const QList<KdosResource *> r = find(f);
    return r.size() == 1 ? r.first() : nullptr;
}

Transaction::Status BackendTest::runToEnd(Transaction *t, QList<int> *progress)
{
    if (!t)
        return Transaction::SetupStatus;
    TransactionModel::global()->addTransaction(t);
    Transaction::Status last = t->status();
    connect(t, &Transaction::statusChanged, this, [&last](Transaction::Status s) {
        last = s;
    });
    if (progress) {
        connect(t, &Transaction::progressChanged, this, [progress, t] {
            progress->append(t->progress());
        });
    }
    // The question a host port's dry run becomes, answered as m_answer says.
    m_asked.clear();
    connect(t, &Transaction::proceedRequest, this, [this, t](const QString &title, const QString &description) {
        m_asked << title << description;
        QTimer::singleShot(0, t, [this, t] {
            if (m_answer)
                t->proceed();
            else
                t->cancel();
        });
    });
    QPointer<Transaction> guard(t);
    for (int i = 0; guard && i < 1000; i++)
        QTest::qWait(20);
    return last;
}

void BackendTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_fixtures = QFileInfo(QFINDTESTDATA("fixtures/catalogue")).absolutePath();
    QVERIFY(!m_fixtures.isEmpty());
    QVERIFY(QDir().mkpath(m_dir.filePath(u"bin"_s)));
    QVERIFY(QDir().mkpath(m_dir.filePath(u"home"_s)));
    QVERIFY(QDir().mkpath(m_dir.filePath(u"medium"_s)));
    QVERIFY(QDir().mkpath(m_dir.filePath(u"store"_s)));

    writeTool(u"kdos-appbox"_s, R"(case "$1" in
catalogue)
	case "$2" in
	--groups) cat "$KDOS_TEST_FIXTURES/groups.tsv" ;;
	--chain) cat "$KDOS_TEST_FIXTURES/chain.tsv" ;;
	*) cat "${KDOS_TEST_CATALOGUE:-$KDOS_TEST_FIXTURES/catalogue.tsv}" ;;
	esac ;;
install)
	echo "==> building kdos/base"
	if [ -n "$KDOS_TEST_FAIL" ]; then echo "  $2: the image did not build." >&2; echo "1 of 1 did not install" >&2; exit 1; fi
	echo "    kdos/rt-kde is already here"
	echo "==> building kdos/$2"
	echo "==> $2 installed" ;;
uninstall)
	echo "==> $2 removed" ;;
import)
	echo "==> app.scribus"
	echo "==> app.kcalc"
	echo "==> 2 imported, 0 failed" ;;
esac)");
    writeTool(u"kdos-box"_s, R"(case "$1" in
list) [ "$2" = --json ] && [ -z "$KDOS_TEST_NOBOXES" ] && cat "$KDOS_TEST_FIXTURES/box-list-machine.json" ;;
profile) cat "$KDOS_TEST_FIXTURES/box-profile.txt" ;;
esac)");
    writeTool(u"kpkg"_s, R"([ "$1 $2" = "list --json" ] && cat "$KDOS_TEST_FIXTURES/kpkg-list.json")");
    writeTool(u"kdos-pack"_s, R"(case "$1" in
extract-meta)
	case "$2" in *broken.kpack) echo "kdos-pack: not a pack" >&2; exit 1 ;; esac
	[ "$3" = --icon ] && exit 1
	cat "$KDOS_TEST_FIXTURES/kdos-pack-meta" ;;
verify)
	echo "app.notes: signed by ab12cd34ef567890" ;;
esac)");
    // The helper's first line comes only once polkit has the password.
    // With KDOS_TEST_BLOCK it is a build the user's signal cannot reach, as
    // root's cannot: it ignores SIGTERM, leaves only when its standard input
    // closes, and marks the file named when it has. Its first line is
    // KDOS_TEST_FIRST's, the package named after it.
    writeTool(u"pkexec"_s, R"sh(if [ -n "$KDOS_TEST_BLOCK" ]; then
	trap '' TERM
	echo "==> $KDOS_TEST_FIRST $3 (built here)"
	cat > /dev/null
	sleep 1
	: > "$KDOS_TEST_BLOCK"
	echo "==> $3: cancelled"
	exit 130
fi
echo "==> $2 $3")sh");
    writeTool(u"kdos"_s, R"sh(if [ -n "$KDOS_TEST_REFUSE" ]; then echo "kdos update: 1 package needs a source build (kdos-addons)" >&2; exit 1; fi
case "$*" in
"update install --dry-run --prebuilt "*) cat "$KDOS_TEST_FIXTURES/install-prebuilt.txt" ;;
"update install --dry-run --source "*) cat "$KDOS_TEST_FIXTURES/install-source.txt" ;;
"update removable --json") [ -n "$KDOS_TEST_REMOVABLE" ] && printf '%s\n' "$KDOS_TEST_REMOVABLE" ;;
"update remove --dry-run "*) [ -z "$KDOS_TEST_REMOVE_SILENT" ] && echo "remove $4" && for p in $KDOS_TEST_REMOVE_ALSO; do echo "remove $p"; done ;;
esac)sh");

    QFile::copy(m_fixtures + u"/apps-pending"_s, m_dir.filePath(u"apps-pending"_s));

    const QString socket = m_dir.filePath(u"packd.sock"_s);
    QVERIFY(m_packd.listen(socket));
    connect(&m_packd, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *c = m_packd.nextPendingConnection()) {
            connect(c, &QLocalSocket::disconnected, c, &QObject::deleteLater);
            connect(c, &QLocalSocket::readyRead, c, [this, c] {
                if (!c->canReadLine())
                    return;
                const QString req = QString::fromUtf8(c->readLine()).trimmed();
                m_packdRequests << req;
                QByteArray answer = "ok done\n";
                if (req == "list"_L1)
                    answer = fixture(u"packd-list"_s);
                else if (req == "info box.work"_L1)
                    answer = fixture(u"packd-info"_s);
                else if (req.startsWith("info "_L1))
                    answer = "err no pack " + req.mid(5).toUtf8() + '\n';
                c->write(answer);
                c->flush();
                c->disconnectFromServer();
            });
        }
    });

    qputenv("PATH", (m_dir.filePath(u"bin"_s) + u':' + qEnvironmentVariable("PATH")).toUtf8());
    qputenv("KDOS_TEST_LOG", m_dir.filePath(u"tools.log"_s).toUtf8());
    qputenv("KDOS_TEST_FIXTURES", m_fixtures.toUtf8());
    qputenv("KDOS_CATALOGUE", (m_fixtures + u"/catalogue"_s).toUtf8());
    qputenv("KDOS_PACKD_SOCKET", socket.toUtf8());
    qputenv("KDOS_PACK_MEDIUM", m_dir.filePath(u"medium"_s).toUtf8());
    qputenv("KDOS_PACK_STORE", m_dir.filePath(u"store"_s).toUtf8());
    qputenv("KDOS_APPS_PENDING", m_dir.filePath(u"apps-pending"_s).toUtf8());
    qputenv("KDOS_SWCATALOG", (m_fixtures + u"/swcatalog"_s).toUtf8());
    qputenv("KDOS_BINHOST_CACHE", (m_fixtures + u"/binhost"_s).toUtf8());
    qputenv("KDOS_DEBIAN_EXCLUDE", (m_fixtures + u"/debian-exclude"_s).toUtf8());
    qputenv("HOME", m_dir.filePath(u"home"_s).toUtf8());
    qputenv("XDG_CACHE_HOME", m_dir.filePath(u"home/.cache"_s).toUtf8());
    qputenv("XDG_DATA_HOME", m_dir.filePath(u"home/.local/share"_s).toUtf8());

    m_backend = new KdosBackend(this);
    m_backend->setName(u"kdos-backend"_s);
    QVERIFY(m_backend->isValid());
    // The first refresh lands without the AppStream pool and the second after
    // it: the Debian boxes are described only once both have.
    QTRY_VERIFY_WITH_TIMEOUT(resource(u"debian"_s, u"gimp"_s), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(m_backend->fetchingUpdatesProgress() == 100, 10000);
}

void BackendTest::catalogueRows()
{
    KdosResource *kcalc = resource(u"catalogue"_s, u"app.kcalc"_s);
    QVERIFY(kcalc);
    QCOMPARE(kcalc->name(), u"KCalc"_s);
    QCOMPARE(kcalc->state(), AbstractResource::Installed);
    QCOMPARE(kcalc->origin(), u"kdos-store"_s); // built by the store lane: no pack
    QCOMPARE(kcalc->boxName(), u"app.kcalc"_s);
    QVERIFY(kcalc->hasCategory(u"kdos-box"_s));
    QVERIFY(kcalc->canExecute());
    QVERIFY(kcalc->verifiedMessage().contains(u"app.kcalc"_s));

    // A row with its pack in the store and a newer one on the medium, no box:
    // installable offline, and described by its Debian package's component.
    KdosResource *scribus = resource(u"catalogue"_s, u"app.scribus"_s);
    QVERIFY(scribus);
    QCOMPARE(scribus->state(), AbstractResource::None);
    QCOMPARE(scribus->entry().packOrigin, u"store"_s);
    QCOMPARE(scribus->availableVersion(), u"1740000000-1"_s);
    QCOMPARE(scribus->homepage(), QUrl(u"https://www.scribus.net"_s));
    QVERIFY(scribus->kdosNotice().contains(u"pack store"_s));
    QVERIFY(!scribus->sizeDescription().contains(u"estimate"_s));

    // No pack anywhere: the store lane, and it says it needs the network.
    KdosResource *tess = resource(u"catalogue"_s, u"data.tess"_s);
    QVERIFY(tess);
    QCOMPARE(tess->type(), AbstractResource::ApplicationSupport);
    QVERIFY(tess->kdosNotice().contains(u"network"_s));
    QVERIFY(tess->kdosNotice().contains(u"estimate"_s));
    QCOMPARE(tess->origin(), u"kdos-store"_s); // built by the store lane
    QVERIFY(tess->sizeDescription().startsWith(u"About "_s));
    // Built here, on a base this machine has not built: the base counts.
    QCOMPARE(tess->size(), quint64(30000000 + 200000000));
    QCOMPARE(scribus->origin(), u"kdos-packs"_s); // its pack is in the store

    // A size nobody knows is no size at all: Discover spins beside a 0 that
    // has a description.
    KdosResource *kate = resource(u"port"_s, u"kate"_s);
    QVERIFY(kate);
    QCOMPARE(kate->size(), quint64(0));
    QVERIFY(kate->sizeDescription().isEmpty());

    // Discover's lists keep one resource per appstreamId and offer the rest
    // of an id's resources as its sources: every resource has its own id but
    // a port built here, which shares the binhost port's so that the two are
    // one application with two origins, and a list names only the one
    // `install =` prefers (hostPorts).
    QSet<QString> ids;
    Filters every;
    const QList<KdosResource *> listed = find(every);
    for (KdosResource *r : listed) {
        QVERIFY(r->entry().lane != Lane::PortSource);
        ids.insert(r->appstreamId());
    }
    QCOMPARE(ids.size(), listed.size());
    QCOMPARE(kate->appstreamId(), u"org.kde.kate"_s);
    QVERIFY(scribus->appstreamId().startsWith(u"io.kdos."_s));
    QCOMPARE(scribus->url(), QUrl(u"kdos-backend://catalogue/app.scribus"_s));

    // The listing's row that is no id is no resource.
    Filters all;
    for (KdosResource *r : find(all))
        QVERIFY(r->entry().id != u"not an id"_s);
}

void BackendTest::sets()
{
    KdosResource *office = resource(u"set"_s, u"office"_s);
    QVERIFY(office);
    QCOMPARE(office->name(), u"Office set"_s);
    QCOMPARE(office->entry().members, (QStringList{u"app.scribus"_s, u"app.kcalc"_s, u"data.tess"_s}));
    QCOMPARE(office->memberNames(), (QStringList{u"Scribus"_s, u"KCalc"_s, u"Tesseract languages"_s}));
    QCOMPARE(office->state(), AbstractResource::None); // scribus is not installed
    QVERIFY(office->hasCategory(u"kdos-set:office"_s));
    // The members' estimates, plus the one base data.tess is built on;
    // scribus has a pack and kcalc is installed, so neither adds a chain.
    QCOMPARE(office->size(), quint64(99000000 + 4200000 + 30000000 + 200000000));
}

void BackendTest::packsAndBoxes()
{
    // A pack no catalogue row names, described by packd's info, with its box.
    KdosResource *work = resource(u"pack"_s, u"box.work"_s);
    QVERIFY(work);
    QCOMPARE(work->name(), u"My work box"_s);
    QCOMPARE(work->state(), AbstractResource::Installed);
    QCOMPARE(work->boxName(), u"work"_s);
    QCOMPARE(work->boxAccent(), u"amber"_s);
    // What the page shows is said for a person, not as kdos-box printed it.
    QCOMPARE(work->boxAccentText(), u"Amber"_s);
    QCOMPARE(work->boxStateText(), u"Stopped"_s);
    QCOMPARE(work->boxPersistenceText(), u"Yes"_s);
    QVERIFY(!work->boxBaseText().startsWith(u"pack:"_s));
    KdosResource *kcalc = resource(u"catalogue"_s, u"app.kcalc"_s);
    QVERIFY(kcalc);
    QCOMPARE(kcalc->boxStateText(), u"Running"_s);
    QCOMPARE(kcalc->boxAccentText(), u"Neutral"_s);
    QVERIFY(!kcalc->boxBaseText().startsWith(u"image:"_s));
    QCOMPARE(work->section(), u"Development"_s);
    QVERIFY(m_packdRequests.contains(u"info box.work"_s));
    QVERIFY(!m_packdRequests.contains(u"info app.scribus"_s)); // the catalogue describes it

    // A runtime in the store is a pack of its own, installed.
    KdosResource *rt = resource(u"pack"_s, u"rt-gtk"_s);
    QVERIFY(rt);
    QCOMPARE(rt->state(), AbstractResource::Installed);
    QCOMPARE(rt->type(), AbstractResource::ApplicationSupport);

    // A box nothing else accounts for.
    KdosResource *scratch = resource(u"box"_s, u"scratch"_s);
    QVERIFY(scratch);
    QCOMPARE(scratch->origin(), u"kdos-boxes"_s);
    QCOMPARE(scratch->size(), quint64(12582912));
    QVERIFY(!resource(u"box"_s, u"work"_s)); // the pack claims it
    QVERIFY(!resource(u"box"_s, u"app.kcalc"_s)); // the catalogue row claims it

    // The page asks for the box's permissions, and only then.
    QVERIFY(!toolLog().contains(u"kdos-box profile work"_s));
    QSignalSpy grants(work, &KdosResource::boxGrantsChanged);
    work->fetchBoxGrants();
    QVERIFY(grants.wait(5000));
    QVERIFY(toolLog().contains(u"kdos-box profile work"_s));
    QStringList labels;
    for (const QVariant &row : work->boxGrants())
        labels << row.toMap().value(u"label"_s).toString();
    QCOMPARE(labels.first(), u"Network:"_s);
    QVERIFY(labels.contains(u"Desktop grants:"_s));
    QCOMPARE(work->boxWarnings().size(), 1);
}

void BackendTest::debianLane()
{
    // A Debian application's box is a resource before any search.
    KdosResource *gimp = resource(u"debian"_s, u"gimp"_s);
    QVERIFY(gimp);
    QCOMPARE(gimp->state(), AbstractResource::Installed);
    QCOMPARE(gimp->origin(), u"kdos-store"_s);
    QCOMPARE(gimp->name(), u"GNU Image Manipulation Program"_s);
    QCOMPARE(gimp->boxName(), u"deb.gimp"_s);

    // Search reaches the rest of the DEP-11 catalogue lazily.
    Filters f;
    f.search = u"inkscape"_s;
    const QList<KdosResource *> ink = find(f);
    QCOMPARE(ink.size(), 1);
    QCOMPARE(ink.first()->entry().lane, Lane::Debian);
    // The profile an uninstall keeps (deb.inkscape, "not created" over its
    // own image) is not a box: the application is not installed.
    QCOMPARE(ink.first()->state(), AbstractResource::None);
    QVERIFY(!ink.first()->hasCategory(u"kdos-box"_s));
    QVERIFY(!resource(u"box"_s, u"deb.inkscape"_s));
    QVERIFY(ink.first()->kdosNotice().contains(u"inkscape"_s));
    // Debian's stock icons are not installed: an icon the theme lacks gives
    // way to the icon of the application's category.
    if (!QIcon::hasThemeIcon(u"inkscape"_s))
        QCOMPARE(ink.first()->entry().icon, u"image-x-generic"_s);

    // A package the exclusion list names is never offered, by search, by
    // category or by link.
    f.search = u"kmag"_s;
    QVERIFY(find(f).isEmpty());

    // A package a catalogue row claims is that row, never a Debian twin.
    f.search = u"scribus"_s;
    const QList<KdosResource *> scribus = find(f);
    QCOMPARE(scribus.size(), 1);
    QCOMPARE(scribus.first()->entry().lane, Lane::Catalogue);

    // An installed-only query never walks the archive.
    f.search = u"inkscape"_s;
    f.state = AbstractResource::Installed;
    QVERIFY(find(f).isEmpty());

    // appstream:// links, as KRunner's software results open Discover: a
    // host port is its two twins, the binhost's first, a package a catalogue
    // row packs is that row, and the rest of the archive is the Debian lane.
    auto byUrl = [this](const QString &url) {
        Filters u;
        u.resourceUrl = QUrl(url);
        const QList<KdosResource *> r = find(u);
        return r.size() == 1 ? r.first() : nullptr;
    };
    Filters kateUrl;
    kateUrl.resourceUrl = QUrl(u"appstream://org.kde.kate"_s);
    const QList<KdosResource *> kate = find(kateUrl);
    QCOMPARE(kate.size(), 2);
    QCOMPARE(kate.first()->entry().lane, Lane::Port);
    QCOMPARE(kate.last()->entry().lane, Lane::PortSource);
    KdosResource *scribusLink = byUrl(u"appstream://net.scribus.Scribus"_s);
    QVERIFY(scribusLink);
    QCOMPARE(scribusLink->entry().lane, Lane::Catalogue);
    QCOMPARE(scribusLink->entry().id, u"app.scribus"_s);
    KdosResource *inkLink = byUrl(u"appstream://org.inkscape.Inkscape"_s);
    QVERIFY(inkLink);
    QCOMPARE(inkLink->entry().lane, Lane::Debian);
    QVERIFY(!byUrl(u"appstream://org.kde.kmag"_s));
}

void BackendTest::hostPorts()
{
    KdosResource *kate = resource(u"port"_s, u"kate"_s);
    QVERIFY(kate);
    QCOMPARE(kate->state(), AbstractResource::Installed);
    QCOMPARE(kate->installedVersion(), u"26.08.2-1"_s);
    QCOMPARE(kate->appstreamId(), u"org.kde.kate"_s);
    QVERIFY(!kate->isRemovable());
    QVERIFY(kate->canExecute());

    // The binhost's catalogue offers what is not installed, once: kate is in
    // both catalogues and stays the installed resource.
    KdosResource *addons = resource(u"port"_s, u"kdos-addons"_s);
    QVERIFY(addons);
    QCOMPARE(addons->state(), AbstractResource::None);
    QCOMPARE(addons->availableVersion(), u"1.0.0-1"_s);
    QVERIFY(addons->kdosNotice().contains(u"binary host"_s));
    // Beside it, the same port built on this computer: a second origin,
    // kept for kate, which is installed, with kate's state.
    KdosResource *addonsBuilt = resource(u"port-source"_s, u"kdos-addons"_s);
    QVERIFY(addonsBuilt);
    QCOMPARE(addonsBuilt->state(), AbstractResource::None);
    QCOMPARE(addonsBuilt->origin(), u"kdos-ports-source"_s);
    QCOMPARE(addonsBuilt->appstreamId(), addons->appstreamId());
    QVERIFY(addonsBuilt->kdosNotice().contains(u"on this computer"_s));
    QVERIFY(!addonsBuilt->isRemovable());
    KdosResource *kateBuilt = resource(u"port-source"_s, u"kate"_s);
    QVERIFY(kateBuilt);
    QCOMPARE(kateBuilt->state(), AbstractResource::Installed);
    QCOMPARE(kateBuilt->installedVersion(), kate->installedVersion());
    // Installed, the twin is the same package: same Remove, same notice.
    QCOMPARE(kateBuilt->isRemovable(), kate->isRemovable());
    QCOMPARE(kateBuilt->kdosNotice(), kate->kdosNotice());

    // Which twin Discover opens follows `install =`: a list names that one
    // alone, and the application's link answers both, that one first, which
    // is the resource Discover opens and the source picker's other entry.
    auto listedLanes = [this](const QString &id) {
        QList<Lane> lanes;
        Filters all;
        for (KdosResource *r : find(all))
            if (r->entry().id == id)
                lanes << r->entry().lane;
        return lanes;
    };
    auto linkLanes = [this, addons] {
        QList<Lane> lanes;
        Filters byUrl;
        byUrl.resourceUrl = QUrl(u"appstream://"_s + addons->appstreamId());
        for (KdosResource *r : find(byUrl))
            lanes << r->entry().lane;
        return lanes;
    };
    QCOMPARE(listedLanes(u"kdos-addons"_s), QList<Lane>{Lane::Port});
    QCOMPARE(linkLanes(), (QList<Lane>{Lane::Port, Lane::PortSource}));
    const QString conf = m_dir.filePath(u"update.conf"_s);
    QFile cf(conf);
    QVERIFY(cf.open(QIODevice::WriteOnly));
    cf.write("# what Discover and the terminal install by\ninstall = source\n");
    cf.close();
    qputenv("KDOS_UPDATE_CONF", conf.toUtf8());
    QCOMPARE(listedLanes(u"kdos-addons"_s), QList<Lane>{Lane::PortSource});
    QCOMPARE(linkLanes(), (QList<Lane>{Lane::PortSource, Lane::Port}));
    // An installed port is one row, whatever the route, and its link
    // answers both twins, the binhost's first.
    QCOMPARE(listedLanes(u"kate"_s), QList<Lane>{Lane::Port});
    {
        QList<Lane> lanes;
        Filters byUrl;
        byUrl.resourceUrl = QUrl(u"appstream://org.kde.kate"_s);
        for (KdosResource *r : find(byUrl))
            lanes << r->entry().lane;
        QCOMPARE(lanes, (QList<Lane>{Lane::Port, Lane::PortSource}));
    }
    // Browsing an origin lists each of its resources.
    Filters byOrigin;
    byOrigin.origin = u"kdos-ports"_s;
    bool binhostTwin = false;
    for (KdosResource *r : find(byOrigin))
        binhostTwin |= r->entry().id == "kdos-addons"_L1;
    QVERIFY(binhostTwin);
    qunsetenv("KDOS_UPDATE_CONF");
    // An install and a removal keep both twins, the same two resources, so
    // the page Discover holds open keeps its source picker through both;
    // while the port is installed the built-here twin carries its state
    // and a list leaves it out.
    writeTool(u"kpkg"_s, R"([ "$1 $2" = "list --json" ] && printf '%s\n' '{"packages":[{"name":"kate","version":"26.08.2","release":"1"},{"name":"kdos-addons","version":"1.0.0","release":"1"}]}')");
    m_backend->checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(addons->state(), AbstractResource::Installed, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(m_backend->fetchingUpdatesProgress() == 100, 10000);
    QCOMPARE(resource(u"port-source"_s, u"kdos-addons"_s), addonsBuilt);
    QCOMPARE(addonsBuilt->state(), AbstractResource::Installed);
    QCOMPARE(addonsBuilt->installedVersion(), addons->installedVersion());
    QCOMPARE(listedLanes(u"kdos-addons"_s), QList<Lane>{Lane::Port});
    QCOMPARE(linkLanes(), (QList<Lane>{Lane::Port, Lane::PortSource}));
    writeTool(u"kpkg"_s, R"([ "$1 $2" = "list --json" ] && cat "$KDOS_TEST_FIXTURES/kpkg-list.json")");
    m_backend->checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(addons->state(), AbstractResource::None, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(m_backend->fetchingUpdatesProgress() == 100, 10000);
    QCOMPARE(resource(u"port"_s, u"kdos-addons"_s), addons);
    QCOMPARE(resource(u"port-source"_s, u"kdos-addons"_s), addonsBuilt);
    QCOMPARE(addonsBuilt->state(), AbstractResource::None);
    // Debian's DEP-11 has a Kate too, under the same component id: the host
    // has it natively, so the Debian lane never offers it in a box.
    Filters f;
    f.search = u"Kate"_s;
    int kates = 0, debianKates = 0;
    for (KdosResource *r : find(f)) {
        kates += r->entry().lane == Lane::Port;
        debianKates += r->entry().lane == Lane::Debian;
    }
    QCOMPARE(kates, 1);
    QCOMPARE(debianKates, 0);
}

// A binhost catalogue as kdos-discover-catalog writes one over a build
// tree: a package of several applications and an add-on, a console
// program, an add-on alone, and a host port whose legacy id Debian's
// DEP-11 spells with ".desktop" where the host's has none, and a package
// whose shortest-named application is metainfo with a .desktop id and no
// description, which the mark says is no bare entry.
static const char ListingCatalogue[] = R"(<?xml version="1.0" encoding="utf-8"?>
<components version="1.0" origin="kdos-binhost">
  <component type="addon">
    <id>org.libreoffice.kde</id>
    <pkgname>libreoffice</pkgname>
    <name>LibreOffice KDE Integration</name>
    <custom><value key="KDOS::Version">26.2.0.3-1</value></custom>
  </component>
  <component type="desktop-application">
    <id>libreoffice-writer.desktop</id>
    <pkgname>libreoffice</pkgname>
    <name>LibreOffice Writer</name>
    <summary>Letters &amp; reports</summary>
    <description><p>A word processor.</p></description>
    <project_license>MPL-2.0</project_license>
    <developer id="org.documentfoundation"><name>The Document Foundation</name></developer>
    <icon type="stock">libreoffice-writer</icon>
    <keywords><keyword>Letter</keyword></keywords>
    <launchable type="desktop-id">libreoffice-writer.desktop</launchable>
    <custom><value key="KDOS::Version">26.2.0.3-1</value></custom>
  </component>
  <component type="desktop-application">
    <id>libreoffice-calc.desktop</id>
    <pkgname>libreoffice</pkgname>
    <name>LibreOffice Calc</name>
    <summary>Tables and charts</summary>
    <description><p>A spreadsheet.</p></description>
    <project_license>MPL-2.0</project_license>
    <developer id="org.documentfoundation"><name>The Document Foundation</name></developer>
    <icon type="stock">libreoffice-calc</icon>
    <launchable type="desktop-id">libreoffice-calc.desktop</launchable>
    <custom><value key="KDOS::Version">26.2.0.3-1</value></custom>
  </component>
  <component type="desktop-application">
    <id>libreoffice-startcenter.desktop</id>
    <pkgname>libreoffice</pkgname>
    <name>LibreOffice</name>
    <summary>The office suite</summary>
    <icon type="stock">libreoffice-startcenter</icon>
    <launchable type="desktop-id">libreoffice-startcenter.desktop</launchable>
    <project_license>MPL-2.0 AND LGPL-3.0-or-later</project_license>
    <custom><value key="KDOS::Version">26.2.0.3-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="console-application">
    <id>org.freedesktop.appstream.cli</id>
    <pkgname>appstream</pkgname>
    <name>AppStream CLI</name>
    <custom><value key="KDOS::Version">1.1.0-1</value></custom>
  </component>
  <component type="addon">
    <id>org.kde.dolphin-plugins</id>
    <extends>org.kde.dolphin</extends>
    <pkgname>dolphin-plugins</pkgname>
    <name>Dolphin Plugins</name>
    <custom><value key="KDOS::Version">26.08.2-1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.inkscape.Inkscape.desktop</id>
    <pkgname>inkscape</pkgname>
    <name>Inkscape</name>
    <custom><value key="KDOS::Version">1.4.2-1</value></custom>
  </component>
  <component>
    <id>org.freedesktop.libfprint</id>
    <pkgname>libfprint</pkgname>
    <name>libfprint</name>
    <description><p>Fingerprint reader support.</p></description>
    <project_license>LGPL-2.1-or-later</project_license>
    <custom><value key="KDOS::Version">1.94.9-1</value></custom>
  </component>
  <component type="desktop-application">
    <id>librewolf.desktop</id>
    <pkgname>librewolf</pkgname>
    <name>LibreWolf</name>
    <project_license>MPL-2.0</project_license>
    <launchable type="desktop-id">librewolf.desktop</launchable>
    <custom><value key="KDOS::Version">147.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.fooview.desktop</id>
    <pkgname>fooview</pkgname>
    <name>Fooview</name>
    <project_license>GPL-3.0-or-later</project_license>
    <launchable type="desktop-id">org.example.fooview.desktop</launchable>
    <custom><value key="KDOS::Version">2.0-1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.fooview.editor</id>
    <pkgname>fooview</pkgname>
    <name>Fooview Editor</name>
    <summary>Edits views</summary>
    <description><p>An editor.</p></description>
    <project_license>GPL-3.0-or-later</project_license>
    <launchable type="desktop-id">org.example.fooview.editor.desktop</launchable>
    <custom><value key="KDOS::Version">2.0-1</value></custom>
  </component>
</components>
)";

void BackendTest::hostCatalogueListing()
{
    const QString root = m_dir.filePath(u"binhost-listing"_s);
    QVERIFY(QDir().mkpath(root + u"/swcatalog/xml"_s));
    QFile cat(root + u"/swcatalog/xml/kdos-binhost.xml"_s);
    QVERIFY(cat.open(QIODevice::WriteOnly));
    cat.write(ListingCatalogue);
    cat.close();
    // The image's catalogue as kdos-ports-helper left it after installing
    // LibreWolf, which `kpkg remove` has since taken off: kpkg's list, which
    // has no librewolf, is what says it is installed.
    QVERIFY(QDir().mkpath(root + u"/image/xml"_s));
    QFile image(root + u"/image/xml/kdos-ports.xml"_s);
    QVERIFY(image.open(QIODevice::WriteOnly));
    image.write(R"(<?xml version="1.0" encoding="utf-8"?>
<components version="1.0" origin="kdos-ports">
  <component type="desktop-application">
    <id>librewolf.desktop</id>
    <pkgname>librewolf</pkgname>
    <name>LibreWolf</name>
    <project_license>MPL-2.0</project_license>
    <launchable type="desktop-id">librewolf.desktop</launchable>
    <custom><value key="KDOS::Version">147.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
</components>
)");
    image.close();

    const QByteArray binhost = qgetenv("KDOS_BINHOST_CACHE");
    const QByteArray swcatalog = qgetenv("KDOS_SWCATALOG");
    qputenv("KDOS_BINHOST_CACHE", root.toUtf8());
    qputenv("KDOS_SWCATALOG", QByteArray(swcatalog + ':' + (root + u"/image"_s).toUtf8()));
    KdosBackend *shared = std::exchange(m_backend, new KdosBackend(this));
    // A failed check returns early: the shared backend, the cache and the
    // catalogue path come back either way, or every later case reads these.
    const auto restore = qScopeGuard([&] {
        delete std::exchange(m_backend, shared);
        qputenv("KDOS_BINHOST_CACHE", binhost);
        qputenv("KDOS_SWCATALOG", swcatalog);
        writeTool(u"kpkg"_s, R"([ "$1 $2" = "list --json" ] && cat "$KDOS_TEST_FIXTURES/kpkg-list.json")");
    });
    m_backend->setName(u"kdos-backend"_s);
    QTRY_VERIFY_WITH_TIMEOUT(resource(u"debian"_s, u"gimp"_s), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(m_backend->fetchingUpdatesProgress() == 100, 10000);

    // One resource per package, never the add-on beside it: a suite is
    // named by its start centre, the bare desktop entry whose name begins its
    // applications', described by the list of its applications and
    // licensed and authored by their metainfo.
    KdosResource *office = resource(u"port"_s, u"libreoffice"_s);
    QVERIFY(office);
    QCOMPARE(office->name(), u"LibreOffice"_s);
    QCOMPARE(office->appstreamId(), u"libreoffice-startcenter.desktop"_s);
    QCOMPARE(office->type(), AbstractResource::Application);
    QCOMPARE(office->author(), u"The Document Foundation"_s);
    QCOMPARE(office->entry().license, u"MPL-2.0"_s);
    const QString suite = office->longDescription();
    // The summary is the page's heading, so the body opens with the list.
    QCOMPARE(office->comment(), u"The office suite"_s);
    QVERIFY2(!suite.contains(u"The office suite"_s), qPrintable(suite));
    QVERIFY2(suite.startsWith(u"<p>The suite holds:</p><ul><li>"_s), qPrintable(suite));
    QVERIFY2(suite.contains(u"<li>Writer: Letters &amp; reports</li>"_s), qPrintable(suite));
    QVERIFY2(suite.contains(u"<li>Calc: Tables and charts</li>"_s), qPrintable(suite));
    QVERIFY2(suite.indexOf(u"Writer"_s) < suite.indexOf(u"Calc"_s), qPrintable(suite));
    QVERIFY2(!suite.contains(u"A spreadsheet."_s) && !suite.contains(u"A word processor."_s), qPrintable(suite));
    // In a marked catalogue an unmarked component is metainfo, described or
    // not, and never a suite's face.
    KdosResource *fooview = resource(u"port"_s, u"fooview"_s);
    QVERIFY(fooview);
    QCOMPARE(fooview->name(), u"Fooview Editor"_s);
    QCOMPARE(fooview->appstreamId(), u"org.example.fooview.editor"_s);
    // The package's other applications find it by their names and keywords.
    for (const QString &term : {u"writer"_s, u"letter"_s, u"LibreOffice"_s}) {
        Filters byTerm;
        byTerm.search = term;
        bool found = false;
        for (KdosResource *r : find(byTerm))
            found |= r->entry().lane == Lane::Port && r->entry().id == "libreoffice"_L1;
        QVERIFY2(found, qPrintable(term));
    }
    // A link to another of its applications opens the package.
    Filters link;
    link.resourceUrl = QUrl(u"appstream://libreoffice-calc.desktop"_s);
    const QList<KdosResource *> byLink = find(link);
    QVERIFY(!byLink.isEmpty());
    QCOMPARE(byLink.first()->entry().id, u"libreoffice"_s);

    // A console program and an add-on are not listed as applications, and
    // with no icon of their own get one of their kind.
    KdosResource *cli = resource(u"port"_s, u"appstream"_s);
    QVERIFY(cli);
    QCOMPARE(cli->type(), AbstractResource::ApplicationSupport);
    QCOMPARE(cli->entry().icon, u"utilities-terminal"_s);
    KdosResource *plugins = resource(u"port"_s, u"dolphin-plugins"_s);
    QVERIFY(plugins);
    QCOMPARE(plugins->type(), AbstractResource::Addon);
    QCOMPARE(plugins->entry().icon, u"preferences-plugin"_s);
    // A generic component with no category gets one of its kind, not the
    // unknown-category icon.
    KdosResource *fprint = resource(u"port"_s, u"libfprint"_s);
    QVERIFY(fprint);
    QCOMPARE(fprint->entry().icon, u"application-x-sharedlib"_s);

    // An application whose catalogue names no category is still on the
    // All Applications page, which matches resources by category.
    KdosResource *inkscape = resource(u"port"_s, u"inkscape"_s);
    QVERIFY(inkscape);
    std::shared_ptr<Category> all;
    for (const auto &c : m_backend->category()) {
        if (c->name() == u"All Applications"_s)
            all = c;
    }
    QVERIFY(all);
    QVERIFY(inkscape->categoryMatches(all));

    // What the binhost can add is never offered again in a box, whichever
    // of the two spellings of an id each catalogue uses.
    Filters f;
    f.search = u"inkscape"_s;
    int ports = 0, debian = 0;
    for (KdosResource *r : find(f)) {
        ports += r->entry().lane == Lane::Port || r->entry().lane == Lane::PortSource;
        debian += r->entry().lane == Lane::Debian;
    }
    QVERIFY(ports >= 1);
    QCOMPARE(debian, 0);

    // A port the image's catalogue names and kpkg does not list is not
    // installed: the binhost offers it again, by both origins.
    KdosResource *wolf = resource(u"port"_s, u"librewolf"_s);
    QVERIFY(wolf);
    QCOMPARE(wolf->state(), AbstractResource::None);
    QCOMPARE(wolf->availableVersion(), u"147.0-1"_s);
    QVERIFY(wolf->installedVersion().isEmpty());
    QVERIFY(wolf->kdosNotice().contains(u"binary host"_s));
    QVERIFY(!wolf->canExecute());
    KdosResource *wolfBuilt = resource(u"port-source"_s, u"librewolf"_s);
    QVERIFY(wolfBuilt);
    QCOMPARE(wolfBuilt->state(), AbstractResource::None);
    // A bare entry's recipe licence is its own; a suite's face takes its
    // metainfo applications'.
    QCOMPARE(wolf->entry().license, u"MPL-2.0"_s);

    // A kpkg that exits 0 with no package array has not answered: the
    // image's catalogue still says what is installed.
    writeTool(u"kpkg"_s, R"([ "$1 $2" = "list --json" ] && echo "kpkg: database busy")");
    delete std::exchange(m_backend, new KdosBackend(this));
    m_backend->setName(u"kdos-backend"_s);
    QTRY_VERIFY_WITH_TIMEOUT(resource(u"debian"_s, u"gimp"_s), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(m_backend->fetchingUpdatesProgress() == 100, 10000);
    writeTool(u"kpkg"_s, R"([ "$1 $2" = "list --json" ] && cat "$KDOS_TEST_FIXTURES/kpkg-list.json")");
    wolf = resource(u"port"_s, u"librewolf"_s);
    QVERIFY(wolf);
    QCOMPARE(wolf->state(), AbstractResource::Installed);
    QCOMPARE(wolf->installedVersion(), u"147.0-1"_s);
}

void BackendTest::unmarkedSuiteListing()
{
    // The same catalogue as the published v0.2 binhost's is written: no
    // KDOS::BareEntry value anywhere, no licence on the start centre, and
    // beside each metainfo application a bare entry of the same id with
    // neither description nor licence, and one for Math, which has no
    // metainfo. The start centre is told from the metainfo applications by
    // its missing description, the suite is still listed by it, and each
    // application is named in its list once.
    QByteArray xml(ListingCatalogue);
    const QByteArray mark = R"(<value key="KDOS::BareEntry">1</value>)";
    QVERIFY(xml.contains(mark));
    xml.replace(mark, QByteArray());
    QVERIFY(!xml.contains("KDOS::BareEntry"));
    const QByteArray licence = "<project_license>MPL-2.0 AND LGPL-3.0-or-later</project_license>";
    QVERIFY(xml.contains(licence));
    xml.replace(licence, QByteArray());
    QByteArray bare;
    for (const char *app : {"writer", "calc", "math", "startcenter"}) {
        const QByteArray id = QByteArray("libreoffice-") + app + ".desktop";
        QByteArray name("LibreOffice");
        if (qstrcmp(app, "startcenter") != 0)
            name += ' ' + QByteArray(app).left(1).toUpper() + QByteArray(app).mid(1);
        bare += R"(  <component type="desktop-application" priority="-2147483647">
    <id>)" + id + R"(</id>
    <pkgname>libreoffice</pkgname>
    <name>)" + name + R"(</name>
    <launchable type="desktop-id">)" + id + R"(</launchable>
  </component>
)";
    }
    QVERIFY(xml.contains("</components>"));
    bare += "</components>";
    xml.replace("</components>", bare);
    const QString root = m_dir.filePath(u"binhost-unmarked"_s);
    QVERIFY(QDir().mkpath(root + u"/swcatalog/xml"_s));
    QFile cat(root + u"/swcatalog/xml/kdos-binhost.xml"_s);
    QVERIFY(cat.open(QIODevice::WriteOnly));
    cat.write(xml);
    cat.close();

    // A failed check returns early: the shared backend and the cache come
    // back either way, or every later case reads this catalogue.
    const QByteArray binhost = qgetenv("KDOS_BINHOST_CACHE");
    qputenv("KDOS_BINHOST_CACHE", root.toUtf8());
    KdosBackend *shared = std::exchange(m_backend, new KdosBackend(this));
    const auto restore = qScopeGuard([&] {
        delete std::exchange(m_backend, shared);
        qputenv("KDOS_BINHOST_CACHE", binhost);
    });
    m_backend->setName(u"kdos-backend"_s);
    QTRY_VERIFY_WITH_TIMEOUT(resource(u"debian"_s, u"gimp"_s), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(m_backend->fetchingUpdatesProgress() == 100, 10000);

    KdosResource *office = resource(u"port"_s, u"libreoffice"_s);
    QVERIFY(office);
    QCOMPARE(office->name(), u"LibreOffice"_s);
    QCOMPARE(office->appstreamId(), u"libreoffice-startcenter.desktop"_s);
    QCOMPARE(office->comment(), u"The office suite"_s);
    QCOMPARE(office->author(), u"The Document Foundation"_s);
    const QString suite = office->longDescription();
    QVERIFY2(suite.startsWith(u"<p>The suite holds:</p><ul><li>"_s), qPrintable(suite));
    QVERIFY2(suite.contains(u"<li>Writer: Letters &amp; reports</li>"_s), qPrintable(suite));
    QVERIFY2(suite.contains(u"<li>Calc: Tables and charts</li>"_s), qPrintable(suite));
    QVERIFY2(!suite.contains(u"A spreadsheet."_s) && !suite.contains(u"A word processor."_s), qPrintable(suite));
    QCOMPARE(suite.count(u"<li>"_s), 2);
    QCOMPARE(office->entry().license, u"MPL-2.0"_s);
}

void BackendTest::pendingAndCategories()
{
    KdosResource *office = resource(u"set"_s, u"office"_s);
    KdosResource *scribus = resource(u"catalogue"_s, u"app.scribus"_s);
    KdosResource *work = resource(u"pack"_s, u"box.work"_s);
    KdosResource *addonsPort = resource(u"port"_s, u"kdos-addons"_s);
    QVERIFY(office && scribus && work && addonsPort);

    // The installer recorded the office group: the set and every member.
    QVERIFY(office->hasCategory(u"kdos-pending"_s));
    QVERIFY(scribus->hasCategory(u"kdos-pending"_s));
    QVERIFY(!work->hasCategory(u"kdos-pending"_s));

    const QList<std::shared_ptr<Category>> cats = m_backend->category();
    std::shared_ptr<Category> pending, sets, addons;
    for (const auto &c : cats) {
        if (c->name() == u"Selected during install"_s)
            pending = c;
        else if (c->name() == u"Sets"_s)
            sets = c;
        else if (c->name() == u"Add-ons"_s)
            addons = c;
    }
    QVERIFY(pending);
    QVERIFY(sets);
    QVERIFY(addons);
    QVERIFY(scribus->categoryMatches(pending));
    QVERIFY(!work->categoryMatches(pending));
    QVERIFY(addonsPort->categoryMatches(addons));

    QStringList setPages;
    for (const auto &sub : sets->subCategories())
        setPages << sub->name();
    QCOMPARE(setPages, (QStringList{u"Games set"_s, u"Office set"_s}));
}

void BackendTest::archiveFile()
{
    const QString path = m_dir.filePath(u"office.ktar"_s);
    {
        KTar tar(path);
        QVERIFY(tar.open(QIODevice::WriteOnly));
        QVERIFY(tar.writeFile(u"SELECTION"_s, fixture(u"SELECTION"_s)));
        QVERIFY(tar.writeFile(u"PACKAGES"_s, fixture(u"PACKAGES"_s)));
        QVERIFY(tar.close());
    }
    Filters f;
    f.resourceUrl = QUrl::fromLocalFile(path);
    const QList<KdosResource *> r = find(f);
    QCOMPARE(r.size(), 1);
    KdosResource *set = r.first();
    QCOMPARE(set->entry().lane, Lane::Archive);
    QCOMPARE(set->entry().members, (QStringList{u"app.scribus"_s, u"app.kcalc"_s}));
    QCOMPARE(set->name(), u"office (office)"_s);
    QCOMPARE(set->entry().signature, u"unsigned"_s);
    QCOMPARE(set->kdosNoticeType(), int(InlineMessage::Warning));
    QCOMPARE(set->size(), quint64(91234567 + 4200000));
    QCOMPARE(set->state(), AbstractResource::None);

    // A file opened is its own page, never a search result.
    Filters s;
    s.search = u"office"_s;
    for (KdosResource *found : find(s))
        QVERIFY(found->entry().lane != Lane::Archive);

    clearLog();
    QList<int> progress;
    QCOMPARE(runToEnd(m_backend->installApplication(set), &progress), Transaction::DoneStatus);
    QVERIFY(toolLog().contains(u"kdos-appbox import "_s + path));
    QCOMPARE(progress.last(), 100);

    // A tar that is not a KDOS set says so on its page and installs nothing.
    const QString other = m_dir.filePath(u"test.ktar"_s);
    {
        KTar tar(other);
        QVERIFY(tar.open(QIODevice::WriteOnly));
        QVERIFY(tar.writeFile(u"hostname"_s, "kdos\n"));
        QVERIFY(tar.close());
    }
    f.resourceUrl = QUrl::fromLocalFile(other);
    const QList<KdosResource *> notSet = find(f);
    QCOMPARE(notSet.size(), 1);
    QVERIFY(notSet.first()->comment().contains(u"not a KDOS set"_s));
    QCOMPARE(notSet.first()->kdosNoticeType(), int(InlineMessage::Error));
    QSignalSpy said(m_backend, &AbstractResourcesBackend::passiveMessage);
    QVERIFY(!m_backend->installApplication(notSet.first()));
    QCOMPARE(said.count(), 1);
    QVERIFY(said.first().first().toString().contains(u"not a KDOS set"_s));
}

void BackendTest::packFile()
{
    const QString path = m_dir.filePath(u"notes.kpack"_s);
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not read by the backend: kdos-pack reads it");
    f.close();

    Filters filter;
    filter.resourceUrl = QUrl::fromLocalFile(path);
    const QList<KdosResource *> r = find(filter);
    QCOMPARE(r.size(), 1);
    KdosResource *notes = r.first();
    QCOMPARE(notes->entry().lane, Lane::PackFile);
    QCOMPARE(notes->name(), u"Notes"_s);
    QCOMPARE(notes->availableVersion(), u"1735000000-2"_s);
    QCOMPARE(notes->entry().signature, u"ab12cd34ef567890"_s);
    QVERIFY(notes->verifiedMessage().startsWith(u"Signed by ab12cd34ef567890"_s));
    QCOMPARE(notes->verifiedIconName(), u"security-high"_s);
    QVERIFY(!notes->isRemovable());

    clearLog();
    QCOMPARE(runToEnd(m_backend->installApplication(notes)), Transaction::DoneStatus);
    const QStringList log = toolLog();
    QVERIFY(log.contains(u"kdos-box import "_s + path + u" as app.notes"_s));
    QVERIFY(log.contains(u"kdos-appbox genlaunchers --packs --user"_s));

    // A file kdos-pack cannot read is a page that says so, not an empty
    // answer that drops Discover back on Home.
    const QString broken = m_dir.filePath(u"broken.kpack"_s);
    QFile b(broken);
    QVERIFY(b.open(QIODevice::WriteOnly));
    b.write("garbage");
    b.close();
    filter.resourceUrl = QUrl::fromLocalFile(broken);
    const QList<KdosResource *> bad = find(filter);
    QCOMPARE(bad.size(), 1);
    QCOMPARE(bad.first()->entry().lane, Lane::PackFile);
    QVERIFY(bad.first()->comment().contains(u"not a KDOS pack"_s));
    QCOMPARE(bad.first()->kdosNoticeType(), int(InlineMessage::Error));
    QSignalSpy said(m_backend, &AbstractResourcesBackend::passiveMessage);
    QVERIFY(!m_backend->installApplication(bad.first()));
    QCOMPARE(said.count(), 1);
}

void BackendTest::installMetered()
{
    // data.tess has no pack: `kdos-appbox install`, metered by its chain
    // (base, data.tess) and the closing line.
    KdosResource *tess = resource(u"catalogue"_s, u"data.tess"_s);
    QVERIFY(tess);
    clearLog();
    QList<int> progress;
    QCOMPARE(runToEnd(m_backend->installApplication(tess), &progress), Transaction::DoneStatus);
    QVERIFY(toolLog().contains(u"kdos-appbox install data.tess"_s));
    QCOMPARE(progress, (QList<int>{33, 66, 100}));

    // The stored pack becomes a box without the store lane.
    KdosResource *scribus = resource(u"catalogue"_s, u"app.scribus"_s);
    QVERIFY(scribus);
    clearLog();
    QCOMPARE(runToEnd(m_backend->installApplication(scribus)), Transaction::DoneStatus);
    QCOMPARE(toolLog().filter(u"kdos-box create"_s), QStringList{u"kdos-box create app.scribus base=pack:app.scribus"_s});
    QVERIFY(toolLog().filter(u"kdos-appbox install"_s).isEmpty());
}

void BackendTest::installFails()
{
    KdosResource *tess = resource(u"catalogue"_s, u"data.tess"_s);
    QVERIFY(tess);
    qputenv("KDOS_TEST_FAIL", "1");
    Transaction *t = m_backend->installApplication(tess);
    QVERIFY(t);
    QSignalSpy txSaid(t, &Transaction::passiveMessage);
    QCOMPARE(runToEnd(t), Transaction::DoneWithErrorStatus);
    qunsetenv("KDOS_TEST_FAIL");
    QCOMPARE(txSaid.count(), 1);
    QVERIFY(txSaid.first().first().toString().contains(u"the image did not build"_s));
    // Two tools' last lines are two clauses, never one run-on sentence.
    QVERIFY(txSaid.first().first().toString().contains(u"the image did not build; 1 of 1 did not install"_s));
    QVERIFY(!txSaid.first().first().toString().endsWith(u'.'));
    // Discover shows that message only behind a generic error, so the page's
    // notice says it, through the refresh the failure caused, until the next
    // install of the resource starts.
    QCOMPARE(tess->kdosNoticeType(), int(InlineMessage::Error));
    QCOMPARE(tess->kdosNotice(), txSaid.first().first().toString());
    QVERIFY(tess->kdosNotice().startsWith(tess->name() + u" was not installed: "_s));
    QCOMPARE(runToEnd(m_backend->installApplication(tess)), Transaction::DoneStatus);
    QVERIFY(!tess->kdosNotice().contains(u"was not installed"_s));
    QCOMPARE(tess->kdosNoticeType(), int(InlineMessage::Information));
}

void BackendTest::removeRuns()
{
    KdosResource *work = resource(u"pack"_s, u"box.work"_s);
    KdosResource *scratch = resource(u"box"_s, u"scratch"_s);
    KdosResource *gimp = resource(u"debian"_s, u"gimp"_s);
    KdosResource *addons = resource(u"port"_s, u"kdos-addons"_s);
    QVERIFY(work && scratch && gimp && addons);

    clearLog();
    QCOMPARE(runToEnd(m_backend->removeApplication(work)), Transaction::DoneStatus);
    QVERIFY(toolLog().contains(u"kdos-box remove work --purge"_s));
    QVERIFY(m_packdRequests.contains(u"remove box.work"_s));

    // A box on its own is asked about before anything runs: Cancel leaves
    // it, and the transaction waits at the question until proceed().
    clearLog();
    m_answer = false;
    QCOMPARE(runToEnd(m_backend->removeApplication(scratch)), Transaction::CancelledStatus);
    m_answer = true;
    QCOMPARE(m_asked.value(0), u"Remove the box scratch?"_s);
    QVERIFY(m_asked.value(1).contains(u"every package installed in it"_s));
    QVERIFY(toolLog().filter(u"kdos-box s"_s).isEmpty());
    QVERIFY(toolLog().filter(u"kdos-box remove"_s).isEmpty());

    clearLog();
    Transaction *boxRemoval = m_backend->removeApplication(scratch);
    QVERIFY(boxRemoval);
    TransactionModel::global()->addTransaction(boxRemoval);
    QSignalSpy question(boxRemoval, &Transaction::proceedRequest);
    QTRY_COMPARE_WITH_TIMEOUT(question.count(), 1, 10000);
    QTest::qWait(300);
    QVERIFY(toolLog().filter(u"kdos-box s"_s).isEmpty());
    QCOMPARE(boxRemoval->status(), Transaction::SetupStatus);
    QPointer<Transaction> boxGuard(boxRemoval);
    boxRemoval->proceed();
    QTRY_VERIFY_WITH_TIMEOUT(toolLog().contains(u"kdos-box remove scratch --purge"_s), 10000);
    QCOMPARE(toolLog().filter(u"kdos-box s"_s), QStringList{u"kdos-box stop scratch"_s});
    QTRY_VERIFY_WITH_TIMEOUT(!boxGuard, 20000);

    clearLog();
    QCOMPARE(runToEnd(m_backend->removeApplication(gimp)), Transaction::DoneStatus);
    QVERIFY(toolLog().contains(u"kdos-appbox uninstall deb:gimp"_s));

    // A host port the binhost holds goes through pkexec and the helper,
    // after its dry run was asked about. While pkexec waits for the
    // password the install is in SetupStatus, Discover's "Waiting", and
    // Downloading only from the helper's first line: by then pkexec has run.
    clearLog();
    Transaction *install = m_backend->installApplication(addons);
    QVERIFY(install);
    QList<Transaction::Status> statuses;
    bool pkexecRanFirst = false;
    connect(install, &Transaction::statusChanged, this, [this, &statuses, &pkexecRanFirst](Transaction::Status s) {
        if (s == Transaction::DownloadingStatus && !statuses.contains(s))
            pkexecRanFirst = !toolLog().filter(u"pkexec "_s).isEmpty();
        statuses << s;
    });
    QCOMPARE(runToEnd(install), Transaction::DoneStatus);
    QVERIFY(statuses.contains(Transaction::DownloadingStatus));
    QVERIFY(pkexecRanFirst);
    QCOMPARE(statuses.value(statuses.indexOf(Transaction::DownloadingStatus) - 1), Transaction::SetupStatus);
    QStringList log = toolLog();
    QVERIFY(log.contains(u"kdos update install --dry-run --prebuilt kdos-addons"_s));
    QVERIFY(log.indexOf(u"kdos update install --dry-run --prebuilt kdos-addons"_s)
            < log.indexOf(u"pkexec /usr/libexec/kdos-discover/kdos-ports-helper install kdos-addons"_s));
    QCOMPARE(m_asked.size(), 2);
    // The narrow dialog elides its title, so the name opens the body, which
    // wraps.
    QCOMPARE(m_asked[0], u"Install?"_s);
    QVERIFY(m_asked[1].startsWith(u"<b>%1</b>: Installs 3 packages; downloads "_s.arg(addons->name().toHtmlEscaped())));
    // The size's number and unit never part at a line break. Discover's
    // dialog reads the text as Text.StyledText, which collapses a raw U+00A0
    // with the other white space and keeps only the entity's.
    QVERIFY(m_asked[1].contains(QRegularExpression(u"\\d&nbsp;[KMG]?i?B"_s)));
    QVERIFY(!m_asked[1].contains(QChar::Nbsp));

    // Built here, the question counts the builds and warns of their length.
    KdosResource *addonsBuilt = resource(u"port-source"_s, u"kdos-addons"_s);
    QVERIFY(addonsBuilt);
    clearLog();
    QCOMPARE(runToEnd(m_backend->installApplication(addonsBuilt)), Transaction::DoneStatus);
    log = toolLog();
    QVERIFY(log.contains(u"kdos update install --dry-run --source kdos-addons"_s));
    QVERIFY(log.indexOf(u"kdos update install --dry-run --source kdos-addons"_s)
            < log.indexOf(u"pkexec /usr/libexec/kdos-discover/kdos-ports-builder install kdos-addons"_s));
    QVERIFY(m_asked.value(1).contains(u"building can take hours"_s));

    // Cancel, and a dry run that refuses: nothing reaches pkexec, and the
    // refusal is the message.
    clearLog();
    m_answer = false;
    QCOMPARE(runToEnd(m_backend->installApplication(addons)), Transaction::CancelledStatus);
    m_answer = true;
    QVERIFY(toolLog().filter(u"pkexec"_s).isEmpty());
    clearLog();
    qputenv("KDOS_TEST_REFUSE", "1");
    Transaction *t = m_backend->installApplication(addons);
    QSignalSpy txSaid(t, &Transaction::passiveMessage);
    QCOMPARE(runToEnd(t), Transaction::DoneWithErrorStatus);
    qunsetenv("KDOS_TEST_REFUSE");
    QVERIFY(m_asked.isEmpty());
    QVERIFY(toolLog().filter(u"pkexec"_s).isEmpty());
    QCOMPARE(txSaid.count(), 1);
    QVERIFY(txSaid.first().first().toString().contains(u"needs a source build"_s));
}

void BackendTest::cancelClosesHelperStdin_data()
{
    QTest::addColumn<QString>("first");
    QTest::newRow("installing") << u"installing"_s;
    QTest::newRow("restoring") << u"restoring the development files of"_s;
}

void BackendTest::cancelClosesHelperStdin()
{
    // Cancel during a build here closes the helper's standard input and
    // leaves the button disabled; the transaction is Cancelled only once the
    // helper has gone, and it said Installing from the helper's first
    // `==> installing` or `==> restoring the development files of` line.
    QFETCH(QString, first);
    KdosResource *addonsBuilt = resource(u"port-source"_s, u"kdos-addons"_s);
    QVERIFY(addonsBuilt);
    const QString gone = m_dir.filePath(u"helper-gone"_s);
    QFile::remove(gone);
    qputenv("KDOS_TEST_BLOCK", gone.toUtf8());
    qputenv("KDOS_TEST_FIRST", first.toUtf8());
    Transaction *t = m_backend->installApplication(addonsBuilt);
    QVERIFY(t);
    QList<Transaction::Status> statuses;
    bool heldAfterCancel = false;
    bool goneWhenCancelled = false;
    connect(t, &Transaction::statusChanged, this, [&statuses, &heldAfterCancel, &goneWhenCancelled, gone, t](Transaction::Status s) {
        statuses << s;
        if (s == Transaction::CommittingStatus && statuses.count(s) == 1) {
            QTimer::singleShot(100, t, [&heldAfterCancel, t] {
                t->cancel();
                heldAfterCancel = t->status() == Transaction::CommittingStatus && !t->isCancellable();
            });
        }
        if (s == Transaction::CancelledStatus)
            goneWhenCancelled = QFile::exists(gone);
    });
    QCOMPARE(runToEnd(t), Transaction::CancelledStatus);
    qunsetenv("KDOS_TEST_BLOCK");
    qunsetenv("KDOS_TEST_FIRST");
    QVERIFY(heldAfterCancel);
    QVERIFY(goneWhenCancelled);
    QVERIFY(statuses.contains(Transaction::DownloadingStatus));
    QVERIFY(statuses.indexOf(Transaction::CommittingStatus) > statuses.indexOf(Transaction::DownloadingStatus));
}

void BackendTest::offlineRefuses()
{
    // Discover's offline sheet shows for every reachability but Online, and
    // a step that downloads is refused for each of them before it starts:
    // NetworkManager with networking off is asleep, which Qt calls Unknown.
    using R = QNetworkInformation::Reachability;
    for (const R r : {R::Disconnected, R::Local, R::Site, R::Unknown})
        QVERIFY2(KdosTransaction::offline(r), qPrintable(QString::number(int(r))));
    QVERIFY(!KdosTransaction::offline(R::Online));
}

void BackendTest::debianNameMatch()
{
    // A Debian component a search offers is one every word names, by its
    // name, id, package or keywords; a hit in its description alone is not.
    AppStream::Component c;
    c.setKind(AppStream::Component::KindDesktopApp);
    c.setId(u"galculator.desktop"_s);
    c.setName(u"Galculator"_s);
    c.setSummary(u"Scientific calculator"_s);
    c.setDescription(u"<p>Computes 12*7 and keeps the previous version of a result.</p>"_s);
    c.setPackageNames({u"galculator"_s});
    c.setKeywords({u"Arithmetic"_s});
    QVERIFY(debianMatches(c, {u"galculator"_s}));
    QVERIFY(debianMatches(c, {u"arithmetic"_s}));
    QVERIFY(debianMatches(c, {u"galc"_s}));
    QVERIFY(!debianMatches(c, {u"12*7"_s}));
    QVERIFY(!debianMatches(c, {u"previous"_s, u"version"_s}));
    QVERIFY(!debianMatches(c, {u"scientific"_s}));
    QVERIFY(!debianMatches(c, {u"galculator"_s, u"tile"_s}));
    QVERIFY(!debianMatches(c, {}));
}

void BackendTest::refusals()
{
    KdosResource *kate = resource(u"port"_s, u"kate"_s);
    KdosResource *scratch = resource(u"box"_s, u"scratch"_s);
    QVERIFY(kate && scratch);
    QSignalSpy said(m_backend, &AbstractResourcesBackend::passiveMessage);
    QVERIFY(!m_backend->removeApplication(kate));
    QCOMPARE(said.count(), 1);
    QVERIFY(said.first().first().toString().contains(u"system version"_s));
    QVERIFY(!m_backend->installApplication(scratch));
    QCOMPARE(said.count(), 2);
}

void BackendTest::urlBeforeFirstRefresh()
{
    // Discover opened on a link, from KRunner or with the featured list
    // cached, asks before the backend has read anything: the answer waits.
    auto *fresh = new KdosBackend(this);
    fresh->setName(u"kdos-backend"_s);
    const QStringList urls{u"kdos-backend://set/office"_s,
                           u"kdos-backend://catalogue/app.scribus"_s,
                           u"kdos-backend://catalogue/app.kcalc"_s,
                           u"appstream://net.scribus.Scribus"_s,
                           u"appstream://org.inkscape.Inkscape"_s};
    QList<QPointer<ResultsStream>> streams;
    QHash<QString, QStringList> keys;
    for (const QString &url : urls) {
        QPointer<ResultsStream> st = fresh->search([&url] {
            Filters f;
            f.resourceUrl = QUrl(url);
            return f;
        }());
        connect(st, &ResultsStream::resourcesFound, this, [&keys, url](const QVector<StreamResult> &found) {
            for (const StreamResult &r : found)
                keys[url] << qobject_cast<KdosResource *>(r.resource)->key();
        });
        streams << st;
    }
    QTRY_VERIFY_WITH_TIMEOUT(std::none_of(streams.cbegin(), streams.cend(), [](const QPointer<ResultsStream> &s) {
                                 return !s.isNull();
                             }),
                             30000);
    QCOMPARE(keys.value(urls[0]), QStringList{u"set/office"_s});
    QCOMPARE(keys.value(urls[1]), QStringList{u"catalogue/app.scribus"_s});
    QCOMPARE(keys.value(urls[2]), QStringList{u"catalogue/app.kcalc"_s});
    QCOMPARE(keys.value(urls[3]), QStringList{u"catalogue/app.scribus"_s});
    QCOMPARE(keys.value(urls[4]), QStringList{u"debian/inkscape"_s});
    delete fresh;
}

void BackendTest::boxesCategoryNeedsABox()
{
    auto hasBoxesPage = [](const QList<std::shared_ptr<Category>> &cats) {
        return std::any_of(cats.cbegin(), cats.cend(), [](const std::shared_ptr<Category> &c) {
            return c->name() == u"Boxes"_s;
        });
    };
    QVERIFY(hasBoxesPage(m_backend->category()));

    qputenv("KDOS_TEST_NOBOXES", "1");
    auto *bare = new KdosBackend(this);
    bare->setName(u"kdos-backend"_s);
    QVERIFY(!hasBoxesPage(bare->category()));
    QTRY_VERIFY_WITH_TIMEOUT(bare->planContext().catalogue && bare->fetchingUpdatesProgress() == 100, 10000);
    QVERIFY(!hasBoxesPage(bare->category()));
    delete bare;
    qunsetenv("KDOS_TEST_NOBOXES");
}

void BackendTest::failedCheckIsSaid()
{
    // The last check could not fetch the release tree: the Updates page says
    // so, with where it looked, above a list that would otherwise read "Up
    // to date"; a later check with no problem takes the message away.
    const QString doc = m_dir.filePath(u"update-problem.json"_s);
    auto write = [&doc](const QByteArray &body) {
        QSaveFile f(doc);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(body);
        QVERIFY(f.commit());
    };
    write(R"({"problem":"fetch-failed","detail":"file:///mnt/bh/kdos-ports.tar.xz could not be downloaded","channel":"file:///mnt/bh","offer":null})");
    const QByteArray had = qgetenv("KDOS_UPDATE_DOC");
    qputenv("KDOS_UPDATE_DOC", doc.toUtf8());
    KdosBackend *shared = std::exchange(m_backend, new KdosBackend(this));
    m_backend->setName(u"kdos-backend"_s);
    QSignalSpy said(m_backend, &AbstractResourcesBackend::inlineMessageChanged);
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    auto message = said.last().at(0).value<QSharedPointer<InlineMessage>>();
    QVERIFY(message);
    QCOMPARE(message->type, InlineMessage::Warning);
    // The tool's words name the channel the sentence names, so they are
    // left out, and the URL is said once.
    QCOMPARE(message->message, u"Updates could not be fetched from file:///mnt/bh."_s);

    // An offline Refresh and a binhost index that does not verify are said
    // too, each in its own words.
    write(R"({"problem":"no-network","detail":"","channel":"https://kdos.example/rel","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    message = said.last().at(0).value<QSharedPointer<InlineMessage>>();
    QVERIFY(message);
    QVERIFY(message->message.contains(u"https://kdos.example/rel"_s));
    QVERIFY(message->message.contains(u"no network"_s));

    write(R"({"problem":"binhost-unverified","detail":"index.sig: no key verifies it","channel":"https://kdos.example/rel","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    message = said.last().at(0).value<QSharedPointer<InlineMessage>>();
    QVERIFY(message);
    QVERIFY(message->message.contains(u"package index"_s));
    QVERIFY(message->message.contains(u"does not verify"_s));
    QVERIFY(message->message.contains(u"no key verifies it"_s));

    // A machine ahead of the release is not cut off from it: the published
    // tree older than its own is no warning.
    write(R"({"problem":"fetch-failed","detail":"the published tree 21b67721d537 is older than this machine's 53a78a241","channel":"file:///mnt/bh","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    QVERIFY(!said.last().at(0).value<QSharedPointer<InlineMessage>>());

    write(R"({"problem":"binhost-unverified","detail":"index.sig: no key verifies it","channel":"https://kdos.example/rel","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);

    // A binhost result names the binhost the check was about, never the
    // channel the tree comes from: a sentence that said GitHub publishes
    // no index would send the reader to the wrong place.
    write(R"({"problem":"binhost-absent","detail":"no binhost is published at file:///root/bh","binhost_base":"file:///root/bh","binhost":null,"channel":"https://github.com/kunaldawn/kdos/releases/latest/download","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    message = said.last().at(0).value<QSharedPointer<InlineMessage>>();
    QVERIFY(message);
    QCOMPARE(message->message, u"No package index is published at file:///root/bh."_s);
    QVERIFY(!message->message.contains(u"github"_s));
    // With no base recorded, the index's own origin; with neither, words.
    write(R"({"problem":"binhost-fetch-failed","detail":"curl: (22) 404","binhost_base":null,"binhost":"https://bh.example/rel","channel":"https://github.com/kunaldawn/kdos/releases/latest/download","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    message = said.last().at(0).value<QSharedPointer<InlineMessage>>();
    QVERIFY(message);
    QVERIFY(message->message.contains(u"https://bh.example/rel"_s));
    QVERIFY(!message->message.contains(u"github"_s));
    write(R"({"problem":"binhost-no-network","detail":"","channel":"https://github.com/kunaldawn/kdos/releases/latest/download","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    message = said.last().at(0).value<QSharedPointer<InlineMessage>>();
    QVERIFY(message);
    QVERIFY(message->message.contains(u"the binary host"_s));
    QVERIFY(!message->message.contains(u"github"_s));

    // A server that did not answer on a machine with a route out is no
    // missing network: its words follow in brackets, and name no URL.
    write(R"({"problem":"binhost-fetch-failed","detail":"the server at 10.0.2.2:18999 did not answer","binhost_base":"https://10.0.2.2:18999","binhost":null,"channel":"https://github.com/kunaldawn/kdos/releases/latest/download","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    message = said.last().at(0).value<QSharedPointer<InlineMessage>>();
    QVERIFY(message);
    QCOMPARE(message->message,
             u"The release's package index could not be fetched from https://10.0.2.2:18999. (the server at 10.0.2.2:18999 did not answer)"_s);
    // No network: the sentence says it, and the tool's words, which name the
    // same URL again, are left out.
    write(R"({"problem":"binhost-no-network","detail":"the network could not be reached for the release index at https://10.0.2.2:18999","binhost_base":"https://10.0.2.2:18999","binhost":null,"channel":"https://github.com/kunaldawn/kdos/releases/latest/download","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    message = said.last().at(0).value<QSharedPointer<InlineMessage>>();
    QVERIFY(message);
    QVERIFY(message->message.contains(u"no network"_s));
    QCOMPARE(message->message.count(u"https://10.0.2.2:18999"_s), 1);
    QVERIFY(!message->message.contains(u'('));

    write(R"({"problem":null,"detail":"","channel":"file:///mnt/bh","offer":null})");
    said.clear();
    m_backend->checkForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(said.size() >= 1, 30000);
    QVERIFY(!said.last().at(0).value<QSharedPointer<InlineMessage>>());

    delete std::exchange(m_backend, shared);
    if (had.isNull())
        qunsetenv("KDOS_UPDATE_DOC");
    else
        qputenv("KDOS_UPDATE_DOC", had);
}

void BackendTest::portRemovedOnRequest()
{
    // `kdos update removable` names a port installed on request that
    // nothing installed needs: it has Remove, its notice says what goes, and
    // the removal is the dry run, then the helper under pkexec. A port it
    // names as needed says by what and has no Remove.
    qputenv("KDOS_TEST_REMOVABLE", R"({"removable":["kate"],"needed":{}})");
    KdosBackend *shared = std::exchange(m_backend, new KdosBackend(this));
    m_backend->setName(u"kdos-backend"_s);
    QTRY_VERIFY_WITH_TIMEOUT(resource(u"port"_s, u"kate"_s) && m_backend->fetchingUpdatesProgress() == 100, 30000);
    KdosResource *kate = resource(u"port"_s, u"kate"_s);
    QCOMPARE(kate->state(), AbstractResource::Installed);
    QVERIFY(kate->isRemovable());
    QVERIFY(kate->kdosNotice().contains(u"Installed on request"_s));
    QVERIFY(!kate->kdosNotice().contains(u"binary host"_s));
    clearLog();
    QCOMPARE(runToEnd(m_backend->removeApplication(kate)), Transaction::DoneStatus);
    const QStringList log = toolLog();
    QVERIFY(log.contains(u"kdos update remove --dry-run kate"_s));
    QVERIFY(log.indexOf(u"kdos update remove --dry-run kate"_s) < log.indexOf(u"pkexec /usr/libexec/kdos-discover/kdos-ports-helper remove kate"_s));
    // The dry run's `remove kate` is the question asked before the password.
    QCOMPARE(m_asked.size(), 2);
    QCOMPARE(m_asked[0], u"Remove?"_s);
    QVERIFY(m_asked[1].startsWith(u"<b>%1</b> is removed"_s.arg(kate->name().toHtmlEscaped())));
    QVERIFY(m_asked[1].contains(u"removed from this computer"_s));
    QVERIFY(!m_asked[1].contains(u"it brought"_s));
    // The packages the dry run takes with the port are counted in the
    // question.
    clearLog();
    qputenv("KDOS_TEST_REMOVE_ALSO", "libkate-extra kate-data");
    QCOMPARE(runToEnd(m_backend->removeApplication(kate)), Transaction::DoneStatus);
    qunsetenv("KDOS_TEST_REMOVE_ALSO");
    QCOMPARE(m_asked.size(), 2);
    QVERIFY(m_asked[1].contains(u"with 2 packages it brought that nothing else needs"_s));
    // Cancel removes nothing; a dry run that names no removal, or refuses,
    // fails before pkexec, the refusal with its reason.
    clearLog();
    m_answer = false;
    QCOMPARE(runToEnd(m_backend->removeApplication(kate)), Transaction::CancelledStatus);
    m_answer = true;
    QVERIFY(toolLog().filter(u"pkexec"_s).isEmpty());
    clearLog();
    qputenv("KDOS_TEST_REMOVE_SILENT", "1");
    QCOMPARE(runToEnd(m_backend->removeApplication(kate)), Transaction::DoneWithErrorStatus);
    qunsetenv("KDOS_TEST_REMOVE_SILENT");
    QVERIFY(m_asked.isEmpty());
    QVERIFY(toolLog().filter(u"pkexec"_s).isEmpty());
    clearLog();
    qputenv("KDOS_TEST_REFUSE", "1");
    Transaction *refused = m_backend->removeApplication(kate);
    QSignalSpy refusedSaid(refused, &Transaction::passiveMessage);
    QCOMPARE(runToEnd(refused), Transaction::DoneWithErrorStatus);
    qunsetenv("KDOS_TEST_REFUSE");
    QVERIFY(m_asked.isEmpty());
    QVERIFY(toolLog().filter(u"pkexec"_s).isEmpty());
    QCOMPARE(refusedSaid.count(), 1);
    QVERIFY(refusedSaid.first().first().toString().contains(u"was not removed"_s));

    qputenv("KDOS_TEST_REMOVABLE", R"({"removable":[],"needed":{"kate":"kdevelop"}})");
    delete std::exchange(m_backend, new KdosBackend(this));
    m_backend->setName(u"kdos-backend"_s);
    QTRY_VERIFY_WITH_TIMEOUT(resource(u"port"_s, u"kate"_s) && m_backend->fetchingUpdatesProgress() == 100, 30000);
    kate = resource(u"port"_s, u"kate"_s);
    QVERIFY(!kate->isRemovable());
    QVERIFY(kate->kdosNotice().contains(u"Needed by kdevelop"_s));

    qunsetenv("KDOS_TEST_REMOVABLE");
    delete std::exchange(m_backend, shared);
}

void BackendTest::shippedExclusions()
{
    // The list the image ships, read as the backend reads it: a Debian
    // system's own configurators mean nothing on KDOS, and KRunner offers
    // nothing this list names.
    const QString path = QFINDTESTDATA("../data/debian-exclude");
    QVERIFY(!path.isEmpty());
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QSet<QString> names;
    for (const QByteArray &raw : f.readAll().split('\n')) {
        const QString line = QString::fromUtf8(raw.left(raw.indexOf('#') < 0 ? raw.size() : raw.indexOf('#'))).trimmed();
        if (!line.isEmpty())
            names.insert(line);
    }
    for (const QString &name : {u"galternatives"_s, u"synaptic"_s, u"gdebi"_s, u"configure-debian"_s, u"kmag"_s})
        QVERIFY2(names.contains(name), qPrintable(name));
}

// Applications a search for a word people type should rank by what they mean,
// not by where the word stands in a name.
static const QByteArray RankingCatalogue = R"(<?xml version="1.0" encoding="utf-8"?>
<components version="1.0" origin="kdos-binhost">
  <component type="desktop-application">
    <id>org.example.atspibrowser.desktop</id>
    <pkgname>rank-atspi</pkgname>
    <name>AT-SPI Browser</name>
    <summary>Inspect the accessibility tree</summary>
    <categories><category>Development</category></categories>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.falkon.desktop</id>
    <pkgname>rank-falkon</pkgname>
    <name>Falkon</name>
    <summary>Web Browser</summary>
    <categories><category>Network</category><category>WebBrowser</category></categories>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.chromium.desktop</id>
    <pkgname>rank-chromium</pkgname>
    <name>Chromium</name>
    <summary>Browse the World Wide Web</summary>
    <categories><category>Network</category><category>WebBrowser</category></categories>
    <keywords><keyword>web</keyword><keyword>browser</keyword><keyword>chrome</keyword></keywords>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.mkchromecast.desktop</id>
    <pkgname>rank-mkchromecast</pkgname>
    <name>mkchromecast</name>
    <summary>Cast audio and video to a Chromecast</summary>
    <categories><category>AudioVideo</category></categories>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.thunderbird.desktop</id>
    <pkgname>rank-thunderbird</pkgname>
    <name>Thunderbird</name>
    <summary>Thunderbird is a free and open source email, newsfeed, chat, and calendaring client</summary>
    <categories><category>Office</category><category>Email</category></categories>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.kmail.desktop</id>
    <pkgname>rank-kmail</pkgname>
    <name>KMail</name>
    <summary>A feature-rich email application</summary>
    <categories><category>Office</category><category>Network</category><category>Email</category></categories>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.accountwizard.desktop</id>
    <pkgname>rank-accountwizard</pkgname>
    <name>Account Wizard</name>
    <summary>Configure accounts</summary>
    <categories><category>Office</category><category>Network</category><category>Email</category></categories>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.ktnef.desktop</id>
    <pkgname>rank-ktnef</pkgname>
    <name>KTnef</name>
    <summary>A viewer/extractor for TNEF files</summary>
    <categories><category>Office</category><category>Network</category><category>Email</category></categories>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.w3m.desktop</id>
    <pkgname>rank-w3m</pkgname>
    <name>Web Browser (w3m)</name>
    <summary>Browse the web on the grid</summary>
    <categories><category>Network</category><category>WebBrowser</category><category>ConsoleOnly</category></categories>
    <keywords><keyword>web</keyword><keyword>browser</keyword></keywords>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
  <component type="desktop-application">
    <id>org.example.aerc.desktop</id>
    <pkgname>rank-aerc</pkgname>
    <name>aerc</name>
    <summary>Read and send mail in a terminal</summary>
    <categories><category>Network</category><category>Email</category><category>ConsoleOnly</category></categories>
    <keywords><keyword>mail</keyword><keyword>email</keyword></keywords>
    <custom><value key="KDOS::Version">1.0-1</value><value key="KDOS::BareEntry">1</value></custom>
  </component>
</components>
)";

void BackendTest::searchRanking()
{
    const QString root = m_dir.filePath(u"binhost-ranking"_s);
    QVERIFY(QDir().mkpath(root + u"/swcatalog/xml"_s));
    QFile cat(root + u"/swcatalog/xml/kdos-binhost.xml"_s);
    QVERIFY(cat.open(QIODevice::WriteOnly));
    cat.write(RankingCatalogue);
    cat.close();
    // Catalogue rows name one main category, as the shipped catalogue does.
    const QString listing = root + u"/catalogue.tsv"_s;
    QFile rows(listing);
    QVERIFY(rows.open(QIODevice::WriteOnly));
    rows.write("app.rank-surf\tSurf\tNetwork\t275196928\trt-gtk\tavailable\tSimple web browser by the suckless community\n"
               "app.rank-claws\tClaws Mail\tNetwork\t11681792\trt-gtk\tavailable\tA lightweight GTK email client\n");
    rows.close();
    const QByteArray binhost = qgetenv("KDOS_BINHOST_CACHE");
    qputenv("KDOS_BINHOST_CACHE", root.toUtf8());
    qputenv("KDOS_TEST_CATALOGUE", listing.toUtf8());
    KdosBackend *shared = std::exchange(m_backend, new KdosBackend(this));
    const auto restore = qScopeGuard([&] {
        delete std::exchange(m_backend, shared);
        qputenv("KDOS_BINHOST_CACHE", binhost);
        qunsetenv("KDOS_TEST_CATALOGUE");
    });
    m_backend->setName(u"kdos-backend"_s);
    QTRY_VERIFY_WITH_TIMEOUT(resource(u"port"_s, u"rank-chromium"_s), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(resource(u"catalogue"_s, u"app.rank-surf"_s), 30000);

    // The binary host's ports a search lists, in Discover's order: by score,
    // then by name.
    auto ranked = [this](const QString &term) {
        Filters f;
        f.search = term;
        QVector<StreamResult> got;
        QPointer<ResultsStream> stream = m_backend->search(f);
        connect(stream, &ResultsStream::resourcesFound, this, [&got](const QVector<StreamResult> &found) {
            got << found;
        });
        for (int i = 0; stream && i < 500; i++)
            QTest::qWait(20);
        std::stable_sort(got.begin(), got.end(), [](const StreamResult &a, const StreamResult &b) {
            if (a.sortScore != b.sortScore)
                return a.sortScore > b.sortScore;
            return a.resource->nameSortKey() < b.resource->nameSortKey();
        });
        QStringList ids;
        for (const StreamResult &r : std::as_const(got)) {
            auto *res = qobject_cast<KdosResource *>(r.resource);
            if (res && res->entry().lane == Lane::Port && res->entry().id.startsWith("rank-"_L1))
                ids << res->entry().id.mid(5);
            else if (res && res->entry().lane == Lane::Catalogue && res->entry().id.startsWith("app.rank-"_L1))
                ids << res->entry().id.mid(9);
        }
        return ids;
    };
    auto before = [](const QStringList &ids, const QString &a, const QString &b) {
        return ids.contains(a) && ids.contains(b) && ids.indexOf(a) < ids.indexOf(b);
    };

    // A web browser, by its category or its keyword, before an application
    // that has the word in its name.
    QStringList ids = ranked(u"browser"_s);
    QVERIFY2(before(ids, u"chromium"_s, u"atspi"_s), qPrintable(ids.join(u' ')));
    QVERIFY2(before(ids, u"falkon"_s, u"atspi"_s), qPrintable(ids.join(u' ')));
    // A catalogue row's one main category is Network: its summary says it
    // is a web browser.
    QVERIFY2(before(ids, u"surf"_s, u"atspi"_s), qPrintable(ids.join(u' ')));
    // A browser that runs in a terminal after the ones that draw a window,
    // though it carries the keyword.
    QVERIFY2(before(ids, u"falkon"_s, u"w3m"_s), qPrintable(ids.join(u' ')));
    QVERIFY2(before(ids, u"surf"_s, u"w3m"_s), qPrintable(ids.join(u' ')));
    // A keyword that is the whole term before a name it is part of.
    ids = ranked(u"chrome"_s);
    QVERIFY2(!ids.isEmpty() && ids.first() == "chromium"_L1, qPrintable(ids.join(u' ')));
    QVERIFY2(before(ids, u"chromium"_s, u"mkchromecast"_s), qPrintable(ids.join(u' ')));
    // A mail application that draws a window before a terminal one whose
    // keyword the term is; the terminal client is still found, and first
    // for its own name.
    // The mail clients lead the applications that only file themselves under
    // Email: a summary that says mail, and a name that holds it, rank higher.
    ids = ranked(u"mail"_s);
    QVERIFY2(ids.mid(0, 3) == (QStringList{u"claws"_s, u"kmail"_s, u"thunderbird"_s}), qPrintable(ids.join(u' ')));
    QVERIFY2(before(ids, u"thunderbird"_s, u"accountwizard"_s), qPrintable(ids.join(u' ')));
    QVERIFY2(before(ids, u"ktnef"_s, u"aerc"_s), qPrintable(ids.join(u' ')));
    ids = ranked(u"aerc"_s);
    QVERIFY2(!ids.isEmpty() && ids.first() == "aerc"_L1, qPrintable(ids.join(u' ')));
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_LOGGING_RULES", "org.kdos.discover.backend.debug=true");
    QGuiApplication app(argc, argv);
    BackendTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "backendtest.moc"
