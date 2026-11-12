/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — the readers, the plans and the meter, on fixtures
 *
 * Every fixture is a tool's output in the shape that tool prints it; a
 * change to a tool's format that breaks a reader fails here before it fails
 * on a machine.
 * ---------------------------------
 */

#include "KdosMeter.h"
#include "KdosParse.h"
#include "KdosPlan.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace Kdos;

static QByteArray fixture(const char *name)
{
    QFile f(QFINDTESTDATA(QString::fromLatin1("fixtures/") + QString::fromLatin1(name)));
    if (!f.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", name);
    return f.readAll();
}

class ModelTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void catalogueFile();
    void catalogueChains();
    void catalogueListing();
    void groupListing();
    void chainListing();
    void packd();
    void meta();
    void boxesJson();
    void boxesTable();
    void boxProfile();
    void kpkg();
    void selectionAndIndex();
    void cve();
    void trayRules();
    void smallRules();
    void plansInstall();
    void plansRemove();
    void plansRefuseUnsafeNames();
    void meter();
};

void ModelTest::catalogueFile()
{
    const Catalogue cat = parseCatalogue(fixture("catalogue"));
    QCOMPARE(cat.offered, (QStringList{u"app.kcalc"_s, u"app.maxima"_s, u"app.early"_s, u"app.loop"_s, u"app.scribus"_s, u"data.tess"_s}));
    QVERIFY(cat.rows.contains(u"rt-kde"_s));
    QCOMPARE(cat.rows.value(u"base"_s).parent, QString());
    QCOMPARE(cat.rows.value(u"alpine"_s).packages, QStringList());
    QCOMPARE(cat.rows.value(u"app.maxima"_s).packages, (QStringList{u"maxima"_s, u"wxmaxima"_s}));

    const CatalogueRow kcalc = cat.rows.value(u"app.kcalc"_s);
    QCOMPARE(kcalc.name, u"KCalc"_s);
    QCOMPARE(kcalc.category, u"Utility"_s);
    QCOMPARE(kcalc.bytes, quint64(4200000));
    QCOMPARE(kcalc.tagline, u"Scientific calculator # a hash in a tagline"_s);
    QCOMPARE(cat.rows.value(u"app.scribus"_s).tagline, u"Page layout|with a pipe"_s);
    // A meta line above its row describes nothing.
    QCOMPARE(cat.rows.value(u"app.early"_s).name, QString());

    QCOMPARE(cat.groups.size(), 2);
    const CatalogueGroup *office = cat.group(u"office"_s);
    QVERIFY(office);
    QCOMPARE(office->description, u"Documents, spreadsheets and reading"_s);
    QCOMPARE(office->members, (QStringList{u"app.scribus"_s, u"app.kcalc"_s, u"data.tess"_s}));
    QVERIFY(cat.group(u"games"_s)->members.isEmpty());
    QVERIFY(!cat.group(u"nope"_s));

    // kdos-appbox's rule: a first row that names members lists them, and the
    // first row that names none is the description, wherever it stands.
    const Catalogue late = parseCatalogue("base base -\napp app.a base a\ngroup g app.a\ngroup g Described late\ngroup g Not this one\n");
    QCOMPARE(late.group(u"g"_s)->members, QStringList{u"app.a"_s});
    QCOMPARE(late.group(u"g"_s)->description, u"Described late"_s);
}

void ModelTest::catalogueChains()
{
    const Catalogue cat = parseCatalogue(fixture("catalogue"));
    QCOMPARE(cat.chainLength(u"app.kcalc"_s), 4); // base, rt-qt, rt-kde, app
    QCOMPARE(cat.chainLength(u"app.scribus"_s), 3);
    QCOMPARE(cat.chainLength(u"data.tess"_s), 2);
    QCOMPARE(cat.chainLength(u"app.maxima"_s), 0); // rt-sci is missing
    QCOMPARE(cat.chainLength(u"app.loop"_s), 0); // its own parent
    QCOMPARE(cat.chainLength(u"absent"_s), 0);
}

void ModelTest::catalogueListing()
{
    const QList<CatalogueListing> l = parseCatalogueListing(fixture("catalogue.tsv"));
    QCOMPARE(l.size(), 3);
    QCOMPARE(l[0].id, u"app.kcalc"_s);
    QVERIFY(l[0].installed);
    QCOMPARE(l[0].parent, u"rt-kde"_s);
    QCOMPARE(l[1].tagline, u"Page layout\twith a tab"_s);
    QVERIFY(!l[1].installed);
    QCOMPARE(l[2].bytes, quint64(30000000));
}

void ModelTest::groupListing()
{
    const QList<CatalogueGroup> g = parseGroupListing(fixture("groups.tsv"));
    QCOMPARE(g.size(), 2);
    QCOMPARE(g[0].members, (QStringList{u"app.scribus"_s, u"app.kcalc"_s, u"data.tess"_s}));
    QVERIFY(g[1].members.isEmpty());
}

void ModelTest::chainListing()
{
    const QHash<QString, ChainRow> c = parseChainListing(
        "base\t200000000\t-\tbuilt\n"
        "rt-qt\t60000000\tbase\tabsent\n"
        "rt-kde\t150000000\trt-qt\tabsent\n"
        "bad id\t1\t-\tabsent\n"
        "short\t1\n");
    QCOMPARE(c.size(), 3);
    QVERIFY(c.value(u"base"_s).built);
    QCOMPARE(c.value(u"rt-kde"_s).parent, u"rt-qt"_s);
    // An application on rt-kde with the base built pays for both runtimes.
    QCOMPARE(unbuiltChainBytes(c, u"rt-kde"_s), quint64(210000000));
    QCOMPARE(unbuiltChainBytes(c, u"base"_s), quint64(0));
    QCOMPARE(unbuiltChainBytes(c, u"rt-unknown"_s), quint64(0));
    QCOMPARE(unbuiltChainBytes(c, QString()), quint64(0));
}

void ModelTest::packd()
{
    const QByteArray list = fixture("packd-list");
    QVERIFY(replyOk(list));
    const QList<PackRow> rows = parsePackList(list);
    QCOMPARE(rows.size(), 4);
    QCOMPARE(rows[1].origin, u"medium"_s);
    QCOMPARE(rows[1].version, u"1740000000-1"_s);
    QCOMPARE(rows[2].kind, u"runtime"_s);
    QCOMPARE(rows[2].state, u"mounted"_s);
    QCOMPARE(rows[0].size, quint64(91234567));

    QString why;
    QVERIFY(!replyOk("err not permitted\n", &why));
    QCOMPARE(why, u"not permitted"_s);
    QVERIFY(!replyOk("app.x\t1\tapp\tinstalled\t1\tstore\n", &why)); // cut short
    QVERIFY(parsePackList("app.x\t1\tapp\tinstalled\t1\tstore\n").isEmpty());
    QVERIFY(replyOk("ok app.x 1-1\n"));
    QVERIFY(!replyOk("", &why));
    QCOMPARE(why, u"no answer"_s);
}

void ModelTest::meta()
{
    const Meta m = parseMeta(fixture("packd-info"));
    QCOMPARE(metaValue(m, u"id"_s), u"box.work"_s);
    QCOMPARE(metaValue(m, u"name"_s), u"My work box"_s);
    QCOMPARE(m.value(u"description"_s), (QStringList{u"First line."_s, u"Second line."_s}));
    QCOMPARE(m.value(u"mime"_s), (QStringList{u"text/plain"_s, u"text/x-c"_s}));
    QCOMPARE(metaValue(m, u"origin"_s), u"store"_s);
    QCOMPARE(firstCategory(metaValue(m, u"category"_s)), u"Development"_s);
    QCOMPARE(metaValue(m, u"absent"_s), QString());
}

void ModelTest::boxesJson()
{
    const QList<BoxRow> b = parseBoxList(fixture("box-list.json"));
    QCOMPARE(b.size(), 3);
    QCOMPARE(b[0].name, u"app.kcalc"_s);
    QCOMPARE(b[0].state, u"running"_s);
    QCOMPARE(b[1].base, u"pack:box.work"_s);
    QCOMPARE(b[1].disk, quint64(1048576));
    QCOMPARE(b[1].accent, u"amber"_s);
    QCOMPARE(b[2].state, u"not created"_s);
    QCOMPARE(b[2].base, u"-"_s); // the document's "" for no recorded base
    QCOMPARE(b[2].disk, quint64(0));
}

void ModelTest::boxesTable()
{
    const QList<BoxRow> b = parseBoxList(fixture("box-list.txt"));
    QCOMPARE(b.size(), 3);
    QCOMPARE(b[0].state, u"running"_s);
    QCOMPARE(b[1].disk, quint64(1048576));
    QCOMPARE(b[2].name, u"deb.gimp"_s);
    QCOMPARE(b[2].state, u"not created"_s);
    QCOMPARE(b[2].persistence, u"persistent"_s);
    QCOMPARE(b[2].disk, quint64(0));
    QCOMPARE(b[2].accent, u"session"_s);
}

void ModelTest::boxProfile()
{
    const BoxGrants g = parseBoxProfile(fixture("box-profile.txt"));
    QCOMPARE(g.values.value(u"network"_s), u"private"_s);
    QCOMPARE(g.values.value(u"home"_s), u"shared"_s);
    QCOMPARE(g.values.value(u"wayland"_s), u"yes"_s);
    QCOMPARE(g.values.value(u"x11"_s), u"auto"_s);
    QCOMPARE(g.values.value(u"memory"_s), u"unlimited"_s);
    QCOMPARE(g.values.value(u"grant"_s), u"screencast, mystery"_s);
    // A grant's explanation line is not a key of its own.
    QVERIFY(!g.values.contains(u"screencast"_s));
    QCOMPARE(g.warnings.size(), 1);
    QVERIFY(g.warnings.first().startsWith(u"grant 'mystery'"_s));

    const BoxGrants none = parseBoxProfile("grant       = none        KWin denies its windows the desktop-wide globals\n");
    QCOMPARE(none.values.value(u"grant"_s), u"none"_s);
}

void ModelTest::kpkg()
{
    const QHash<QString, QString> p = parseKpkgList(fixture("kpkg-list.json"));
    QCOMPARE(p.size(), 2);
    QCOMPARE(p.value(u"konsole"_s), u"26.08.2-2"_s);
    QVERIFY(parseKpkgList("not json").isEmpty());
}

void ModelTest::selectionAndIndex()
{
    const Selection s = parseSelection(fixture("SELECTION"));
    QCOMPARE(s.groups, QStringList{u"office"_s});
    // Each id once, and a line that would read as an option is no id.
    QCOMPARE(s.ids, (QStringList{u"app.scribus"_s, u"app.kcalc"_s}));

    const QList<IndexRow> idx = parsePackIndex(fixture("PACKAGES"));
    QCOMPARE(idx.size(), 2);
    QCOMPARE(idx[0].name, u"Scribus"_s);
    QCOMPARE(idx[0].size, quint64(91234567));
    QCOMPARE(idx[1].id, u"app.kcalc"_s);
    QCOMPARE(idx[1].file, u"app.kcalc.kpack"_s);
}

void ModelTest::cve()
{
    QCOMPARE(countCveFindings(fixture("cve.json")), 2);
    QCOMPARE(countCveFindings("{\"findings\": []}"), 0);
    QCOMPARE(countCveFindings("kdos: no security database"), -1);
}

void ModelTest::trayRules()
{
    QString v;
    QVERIFY(updateOffer(fixture("update-offer.json"), &v));
    QCOMPARE(v, u"0.2.14"_s);
    QVERIFY(!updateOffer(fixture("update-none.json")));
    QVERIFY(!updateOffer(""));

    const auto armed = QJsonDocument::fromJson(fixture("bootctl-armed.json")).object().toVariantMap();
    const auto trial = QJsonDocument::fromJson(fixture("bootctl-trial.json")).object().toVariantMap();
    QVERIFY(slotArmed(armed));
    QVERIFY(!slotArmed(trial)); // booted into the trial: nothing to restart for
    QVERIFY(!slotArmed({}));
    QVERIFY(!slotArmed(QVariantMap{{u"configured"_s, false}}));

    // bootctl's document carries no offer: the tray falls back to update.json.
    const SlotsState fromBootctl = readSlotsState(armed);
    QVERIFY(!fromBootctl.offerKnown);
    QVERIFY(fromBootctl.rebootRequired);

    // ...under kdos-sysupdated's CheckNew rules. The fixture's document was
    // made on 0.2.13 and offers 0.2.14.
    QVariantMap boot = armed;
    boot.insert(u"trying"_s, QVariant());
    boot.insert(u"slots"_s, QVariantMap{{u"a"_s, QVariantMap{{u"version"_s, u"0.2.13"_s}}}, {u"b"_s, QVariantMap{{u"version"_s, u"0.2.12"_s}}}});
    QVERIFY(updateOffer(fixture("update-offer.json"), readSlotsState(boot), &v));
    QCOMPARE(v, u"0.2.14"_s);
    QVariantMap otherRoot = boot;
    otherRoot.insert(u"slots"_s, QVariantMap{{u"a"_s, QVariantMap{{u"version"_s, u"0.2.14"_s}}}});
    QVERIFY(!updateOffer(fixture("update-offer.json"), readSlotsState(otherRoot))); // not this root's document
    QVERIFY(!updateOffer(fixture("update-offer.json"), readSlotsState(trial))); // a trial boot
    QVariantMap armedWithOffer = armed;
    armedWithOffer.insert(u"slots"_s, QVariantMap{{u"a"_s, QVariantMap{{u"version"_s, u"0.2.13"_s}}}, {u"b"_s, QVariantMap{{u"version"_s, u"0.2.14"_s}}}});
    QVERIFY(!updateOffer(fixture("update-offer.json"), readSlotsState(armedWithOffer))); // armed with it already
    QVERIFY(updateOffer(fixture("update-offer.json"), readSlotsState({}))); // no boot state: the document alone

    // io.kdos.Slots1's State: its own keys win over the bootctl-shaped ones.
    const auto stateArmed = QJsonDocument::fromJson(fixture("slots-state-armed.json")).object().toVariantMap();
    const SlotsState s = readSlotsState(stateArmed);
    QVERIFY(s.offerKnown);
    QVERIFY(s.offer.isEmpty()); // the armed slot already holds the offer
    QVERIFY(s.rebootRequired);
    QVERIFY(s.job.isEmpty());
    QVariantMap contradicts = stateArmed;
    contradicts.insert(u"rebootRequired"_s, false);
    QVERIFY(!readSlotsState(contradicts).rebootRequired);

    const SlotsState acquiring = readSlotsState(QJsonDocument::fromJson(fixture("slots-state-acquiring.json")).object().toVariantMap());
    QCOMPARE(acquiring.offer, u"0.2.14"_s);
    QCOMPARE(acquiring.job, u"acquire"_s);
    QCOMPARE(acquiring.jobDone, quint64(12));
    QCOMPARE(acquiring.jobTotal, quint64(40));
    QVERIFY(!acquiring.rebootRequired);

    // curl 8.10.1 closes curl's finding (fixed in 8.10.0); expat goes to
    // 2.6.2 and its fix is 2.6.3, so that hole stays open and is no fix.
    QCOMPARE(securityFixes(fixture("update-offer.json"), fixture("cve.json")), 1);
    QCOMPARE(securityFixes(fixture("update-none.json"), fixture("cve.json")), 0);
    QCOMPARE(securityFixes(fixture("update-offer.json"), "{\"findings\": []}"), 0);
    QCOMPARE(securityFixes(fixture("update-offer.json"), "kdos: no security database"), -1);
    QCOMPARE(securityFixes("", fixture("cve.json")), -1);
    const QByteArray noFixVersion = R"({"findings": [{"port": "zlib", "version": "1.3", "fixed_in": "", "cves": "CVE-2026-0003"}]})";
    QCOMPARE(securityFixes(fixture("update-offer.json"), noFixVersion), 1);
}

void ModelTest::smallRules()
{
    QVERIFY(isSafeId(u"app.hugin"_s));
    QVERIFY(isSafeId(u"rt-kde"_s));
    QVERIFY(isSafeId(u"_x"_s));
    QVERIFY(!isSafeId(u"-rf"_s));
    QVERIFY(!isSafeId(u".hidden"_s));
    QVERIFY(!isSafeId(u"a/b"_s));
    QVERIFY(!isSafeId(u"a b"_s));
    QVERIFY(!isSafeId(QString(64, u'a')));
    QVERIFY(isSafeId(QString(63, u'a')));

    QVERIFY(isDebianPackage(u"g++"_s));
    QVERIFY(isDebianPackage(u"libreoffice-writer"_s));
    QVERIFY(!isDebianPackage(u"Upper"_s));
    QVERIFY(!isDebianPackage(u"-x"_s));

    QCOMPARE(boxNameForPack(u"box.work"_s), u"work"_s);
    QCOMPARE(boxNameForPack(u"app.gimp"_s), u"app.gimp"_s);
    QCOMPARE(boxNameForPack(u"box."_s), u"box."_s);
    QCOMPARE(boxNameForDebian(u"g++"_s), u"deb.g__"_s);
    QVERIFY(isSafeId(boxNameForDebian(u"g++"_s)));

    QVERIFY(compareVersions(u"1.10"_s, u"1.9"_s) > 0);
    QVERIFY(compareVersions(u"1740000000-1"_s, u"1730000000-1"_s) > 0);
    QCOMPARE(compareVersions(u"2.0"_s, u"2.0"_s), 0);

    QCOMPARE(parseHumanSize(u"12M"_s), quint64(12582912));
    QCOMPARE(parseHumanSize(u"1.5K"_s), quint64(1536));
    QCOMPARE(parseHumanSize(u"512B"_s), quint64(512));
    QCOMPARE(parseHumanSize(u"-"_s), quint64(0));

    QCOMPARE(firstCategory(u"AudioVideo;Audio;"_s), u"AudioVideo"_s);
    QCOMPARE(firstCategory(QString()), u"Other"_s);
    QCOMPARE(originOf(Lane::Pack, u"medium"_s), u"medium"_s);
    QCOMPARE(originOf(Lane::Debian), u"kdos-store"_s);
    // A catalogue row is named by where its pack is, and with none by the
    // store lane that builds it.
    QCOMPARE(originOf(Lane::Catalogue, u"store"_s), u"kdos-packs"_s);
    QCOMPARE(originOf(Lane::Catalogue, u"medium"_s), u"medium"_s);
    QCOMPARE(originOf(Lane::Catalogue), u"kdos-store"_s);
    QCOMPARE(originOf(Lane::Set), u"kdos-packs"_s);
}

static PlanContext context(const Catalogue *cat)
{
    PlanContext ctx;
    ctx.catalogue = cat;
    ctx.mediumPacks = u"/mnt/iso/packs"_s;
    ctx.packs.insert(u"app.scribus"_s, PackRow{u"app.scribus"_s, u"1"_s, u"app"_s, u"installed"_s, u"store"_s, 1});
    ctx.packs.insert(u"data.tess"_s, PackRow{u"data.tess"_s, u"1"_s, u"data"_s, u"available"_s, u"medium"_s, 1});
    ctx.boxes.insert(u"app.kcalc"_s, BoxRow{u"app.kcalc"_s, u"image:kdos/app.kcalc"_s, u"running"_s, u"persistent"_s, u"session"_s, 0});
    ctx.boxes.insert(u"app.scribus"_s, BoxRow{u"app.scribus"_s, u"pack:app.scribus"_s, u"exited"_s, u"persistent"_s, u"session"_s, 0});
    ctx.boxes.insert(u"work"_s, BoxRow{u"work"_s, u"pack:box.work"_s, u"exited"_s, u"persistent"_s, u"amber"_s, 0});
    return ctx;
}

static QStringList argvOf(const Step &s)
{
    if (s.kind == Step::Kind::Packd)
        return {u"packd"_s, s.request};
    return QStringList{s.program} + s.arguments;
}

void ModelTest::plansInstall()
{
    const Catalogue cat = parseCatalogue(fixture("catalogue"));
    const PlanContext ctx = context(&cat);

    Entry e;
    e.lane = Lane::Catalogue;
    e.id = u"app.scribus"_s;
    Plan p = installPlan(e, ctx);
    QVERIFY(p.isValid());
    QCOMPARE(p.steps.size(), 2);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"create"_s, u"app.scribus"_s, u"base=pack:app.scribus"_s}));
    QCOMPARE(argvOf(p.steps[1]), (QStringList{u"kdos-appbox"_s, u"genlaunchers"_s, u"--packs"_s, u"--user"_s}));
    QVERIFY(!p.steps[0].downloads);

    // A data row whose pack is on the medium: copied into the store, then grafted.
    e.id = u"data.tess"_s;
    p = installPlan(e, ctx);
    QVERIFY(p.isValid());
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"import"_s, u"/mnt/iso/packs/data.tess.kpack"_s}));
    QCOMPARE(argvOf(p.steps[1]), (QStringList{u"packd"_s, u"graft data.tess"_s}));

    // No pack anywhere: the store lane, metered by its chain.
    e.id = u"app.kcalc"_s;
    p = installPlan(e, ctx);
    QVERIFY(p.isValid());
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-appbox"_s, u"install"_s, u"app.kcalc"_s}));
    QCOMPARE(p.steps[0].meter, Meter::AppboxInstall);
    QCOMPARE(p.steps[0].units, 5);
    QVERIFY(p.steps[0].downloads);

    e.id = u"app.maxima"_s;
    QVERIFY(!installPlan(e, ctx).isValid()); // no chain

    Entry set;
    set.lane = Lane::Set;
    set.id = u"office"_s;
    set.members = {u"app.scribus"_s, u"app.kcalc"_s};
    p = installPlan(set, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-appbox"_s, u"install"_s, u"office"_s}));
    QCOMPARE(p.steps[0].units, 2);
    set.members.clear();
    QVERIFY(!installPlan(set, ctx).isValid());

    Entry deb;
    deb.lane = Lane::Debian;
    deb.packages = {u"gimp"_s};
    p = installPlan(deb, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-appbox"_s, u"install"_s, u"deb:gimp"_s}));

    Entry pack;
    pack.lane = Lane::Pack;
    pack.id = u"box.work"_s;
    pack.kind = u"app"_s;
    pack.packOrigin = u"store"_s;
    p = installPlan(pack, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"create"_s, u"work"_s, u"base=pack:box.work"_s}));
    pack.packOrigin = u"medium"_s;
    p = installPlan(pack, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"import"_s, u"/mnt/iso/packs/box.work.kpack"_s, u"as"_s, u"work"_s}));
    pack.packOrigin = u"store"_s;
    pack.kind = u"runtime"_s;
    QVERIFY(!installPlan(pack, ctx).isValid()); // a stored runtime is already all it can be
    pack.state = EntryState::Upgradeable;
    p = installPlan(pack, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"import"_s, u"/mnt/iso/packs/box.work.kpack"_s}));

    Entry box;
    box.lane = Lane::Box;
    box.id = u"work"_s;
    QVERIFY(!installPlan(box, ctx).isValid());

    Entry port;
    port.lane = Lane::Port;
    port.id = u"kate"_s;
    p = installPlan(port, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"pkexec"_s, u"/usr/libexec/kdos-discover/kdos-ports-helper"_s, u"install"_s, u"kate"_s}));
    QVERIFY(p.steps[0].downloads); // the binhost's packages are fetched for it

    QTemporaryDir dir;
    const QString ktar = dir.filePath(u"set.ktar"_s);
    QFile f(ktar);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();
    Entry archive;
    archive.lane = Lane::Archive;
    archive.path = ktar;
    archive.members = {u"app.scribus"_s, u"app.kcalc"_s};
    p = installPlan(archive, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-appbox"_s, u"import"_s, ktar}));
    QCOMPARE(p.steps[0].meter, Meter::AppboxImport);
    archive.members.clear();
    p = installPlan(archive, ctx);
    QVERIFY(!p.isValid()); // no SELECTION: not a set
    QVERIFY(p.refusal.contains(u"not a KDOS set"_s));
    archive.members = {u"app.scribus"_s};
    archive.path = u"relative.ktar"_s;
    QVERIFY(!installPlan(archive, ctx).isValid());
    archive.path = dir.filePath(u"absent.ktar"_s);
    QVERIFY(!installPlan(archive, ctx).isValid());

    Entry file;
    file.lane = Lane::PackFile;
    file.path = ktar;
    file.id = u"data.tess"_s;
    file.kind = u"data"_s;
    p = installPlan(file, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"import"_s, ktar}));
    QCOMPARE(argvOf(p.steps[1]), (QStringList{u"packd"_s, u"graft data.tess"_s}));
    file.signature = u"refused"_s;
    p = installPlan(file, ctx);
    QVERIFY(!p.isValid());
    QVERIFY(p.refusal.contains(u"not a KDOS pack"_s));
}

void ModelTest::plansRemove()
{
    const Catalogue cat = parseCatalogue(fixture("catalogue"));
    const PlanContext ctx = context(&cat);

    Entry e;
    e.lane = Lane::Catalogue;
    e.id = u"app.scribus"_s;
    // Every box is stopped before it goes: kdos-box remove refuses a running one.
    Plan p = removePlan(e, ctx);
    QCOMPARE(p.steps.size(), 4);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"stop"_s, u"app.scribus"_s}));
    QCOMPARE(argvOf(p.steps[1]), (QStringList{u"kdos-box"_s, u"remove"_s, u"app.scribus"_s}));
    QCOMPARE(argvOf(p.steps[2]), (QStringList{u"packd"_s, u"remove app.scribus"_s}));
    QCOMPARE(argvOf(p.steps[3]), (QStringList{u"kdos-appbox"_s, u"genlaunchers"_s, u"--packs"_s, u"--user"_s}));

    e.id = u"app.kcalc"_s; // running
    p = removePlan(e, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"stop"_s, u"app.kcalc"_s}));
    QCOMPARE(argvOf(p.steps[1]), (QStringList{u"kdos-appbox"_s, u"uninstall"_s, u"app.kcalc"_s}));

    e.id = u"data.tess"_s;
    QVERIFY(!removePlan(e, ctx).isValid()); // on the medium only: nothing installed

    Entry box;
    box.lane = Lane::Box;
    box.id = u"work"_s;
    p = removePlan(box, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"stop"_s, u"work"_s}));
    QCOMPARE(argvOf(p.steps[1]), (QStringList{u"kdos-box"_s, u"remove"_s, u"work"_s}));

    Entry pack;
    pack.lane = Lane::Pack;
    pack.id = u"box.work"_s;
    pack.kind = u"app"_s;
    pack.packOrigin = u"store"_s;
    p = removePlan(pack, ctx);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"stop"_s, u"work"_s}));
    QCOMPARE(argvOf(p.steps[1]), (QStringList{u"kdos-box"_s, u"remove"_s, u"work"_s}));
    QCOMPARE(argvOf(p.steps[2]), (QStringList{u"packd"_s, u"remove box.work"_s}));

    Entry port;
    port.lane = Lane::Port;
    port.id = u"kate"_s;
    QVERIFY(!removePlan(port, ctx).isValid());

    Entry deb;
    deb.lane = Lane::Debian;
    deb.packages = {u"gimp"_s};
    QCOMPARE(argvOf(removePlan(deb, ctx).steps[0]), (QStringList{u"kdos-appbox"_s, u"uninstall"_s, u"deb:gimp"_s}));
    PlanContext withBox = ctx;
    withBox.boxes.insert(u"deb.gimp"_s, BoxRow{u"deb.gimp"_s, u"image:kdos/deb.gimp"_s, u"running"_s, u"persistent"_s, u"session"_s, 0});
    p = removePlan(deb, withBox);
    QCOMPARE(argvOf(p.steps[0]), (QStringList{u"kdos-box"_s, u"stop"_s, u"deb.gimp"_s}));
    QCOMPARE(argvOf(p.steps[1]), (QStringList{u"kdos-appbox"_s, u"uninstall"_s, u"deb:gimp"_s}));
}

void ModelTest::plansRefuseUnsafeNames()
{
    const Catalogue cat = parseCatalogue(fixture("catalogue"));
    const PlanContext ctx = context(&cat);
    for (const Lane lane : {Lane::Catalogue, Lane::Set, Lane::Pack, Lane::Port, Lane::Box}) {
        Entry e;
        e.lane = lane;
        e.id = u"--all"_s;
        e.members = {u"x"_s};
        QVERIFY2(!installPlan(e, ctx).isValid() || lane == Lane::Box, qPrintable(originOf(lane)));
        QVERIFY(!removePlan(e, ctx).isValid());
    }
    Entry deb;
    deb.lane = Lane::Debian;
    deb.packages = {u"--force"_s};
    QVERIFY(!installPlan(deb, ctx).isValid());
    QVERIFY(!removePlan(deb, ctx).isValid());
}

void ModelTest::meter()
{
    StepMeter m(Meter::AppboxInstall, 5);
    QVERIFY(m.feed(u"==> building kdos/base"_s));
    QVERIFY(m.feed(u"    kdos/rt-qt is already here"_s));
    QVERIFY(!m.feed(u"STEP 1/2: FROM kdos/rt-qt"_s)); // podman's own chatter moves nothing
    QVERIFY(m.feed(u"==> building kdos/rt-kde"_s));
    QVERIFY(m.feed(u"==> building kdos/app.kcalc"_s));
    QCOMPARE(m.done(), 4);
    QVERIFY(!m.feed(u"==> building kdos/extra"_s)); // the last unit is the closing line's
    QVERIFY(!m.finished());
    QVERIFY(m.feed(u"==> app.kcalc installed"_s));
    QCOMPARE(m.done(), 5);
    QVERIFY(m.finished());

    StepMeter set(Meter::AppboxSet, 2);
    QVERIFY(!set.feed(u"==> building kdos/base"_s));
    QVERIFY(set.feed(u"==> app.scribus installed"_s));
    QVERIFY(set.feed(u"==> app.kcalc installed"_s));
    QVERIFY(!set.feed(u"==> app.more installed"_s));
    QCOMPARE(set.done(), 2);

    StepMeter imp(Meter::AppboxImport, 2);
    QVERIFY(imp.feed(u"==> app.scribus"_s));
    QVERIFY(!imp.feed(u"  app.kcalc: not in this archive"_s));
    QVERIFY(imp.feed(u"==> 1 imported, 1 failed"_s));
    QVERIFY(imp.finished());
    QCOMPARE(imp.done(), 2);
}

QTEST_GUILESS_MAIN(ModelTest)

#include "modeltest.moc"
