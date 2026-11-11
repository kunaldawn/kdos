/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * The Boxes module against the REAL kdos-box, built from
 * src/system/kdos-appbox, over a fake container engine (boxfake).
 *
 * The real program is the point: the page writes nothing itself, so what a
 * test asserts on is the profile file kdos-box wrote and the report
 * kdos-box prints, and a change in kdos-box's format fails here first. Only
 * podman and distrobox are fakes, recording every call in calls.log, so the
 * order of a recreate is asserted on what reached the engine.
 *
 * Each test gets its own $HOME, $XDG_RUNTIME_DIR and $XDG_STATE_HOME under
 * a temporary directory, and a $PATH whose first entry is a directory of
 * links: kdos-box to the real binary, podman, distrobox and kdos-appbox to
 * the fake.
 */

#include "accentmodel.h"
#include "boxesmodule.h"
#include "boxlistmodel.h"
#include "boxprofile.h"
#include "boxtool.h"
#include "confinement.h"

#include <KPluginMetaData>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

class BoxesTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_root;
    std::unique_ptr<Kdos::BoxesModule> m_kcm;
    QByteArray m_path;

    QString root(const QString &rel) const
    {
        return m_root->path() + u'/' + rel;
    }

    void put(const QString &rel, const QString &text) const
    {
        QDir().mkpath(QFileInfo(root(rel)).path());
        QFile f(root(rel));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(text.toUtf8());
    }

    QString get(const QString &rel) const
    {
        QFile f(root(rel));
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    }

    QString calls() const
    {
        return get(u"fake/calls.log"_s);
    }

    QString profileFile(const QString &box) const
    {
        return get(u"home/.config/kdos/boxes/"_s + box + u".conf"_s);
    }

    /* What the real kdos-box reports for a box, as a person would see it. */
    QString report(const QString &box) const
    {
        QProcess p;
        p.start(u"kdos-box"_s, {u"profile"_s, box});
        p.waitForFinished(10000);
        return QString::fromUtf8(p.readAllStandardOutput());
    }

    void machine()
    {
        m_root = std::make_unique<QTemporaryDir>();
        QVERIFY(m_root->isValid());
        for (const char *d : {"home", "run", "state", "bin", "fake"}) {
            QVERIFY(QDir().mkpath(root(QString::fromLatin1(d))));
        }
        QFile::setPermissions(root(u"run"_s), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QVERIFY(QFile::link(QStringLiteral(KDOS_APPBOX_PATH), root(u"bin/kdos-box"_s)));
        for (const char *n : {"podman", "distrobox", "kdos-appbox"}) {
            QVERIFY(QFile::link(QStringLiteral(BOXFAKE_PATH), root(u"bin/"_s + QString::fromLatin1(n))));
        }
        qputenv("HOME", root(u"home"_s).toLocal8Bit());
        qputenv("XDG_RUNTIME_DIR", root(u"run"_s).toLocal8Bit());
        qputenv("XDG_STATE_HOME", root(u"state"_s).toLocal8Bit());
        qputenv("XDG_DATA_HOME", root(u"home/.local/share"_s).toLocal8Bit());
        qputenv("KDOS_FAKE_DIR", root(u"fake"_s).toLocal8Bit());
        qputenv("PATH", QByteArray(root(u"bin"_s).toLocal8Bit() + ':' + m_path));
        qunsetenv("KDOS_FAKE_EXIT");

        /* Three boxes: a running pack box, a stopped image box, and a box
         * that is described and has never been created. */
        put(u"fake/podman.state"_s,
            u"container app.gimp running\n"
            "container dev exited\n"
            "image docker.io/library/debian:trixie\n"_s);
        put(u"home/.config/kdos/boxes/app.gimp.conf"_s, u"base=pack:app.gimp\naccent=amber\n"_s);
        put(u"home/.config/kdos/boxes/dev.conf"_s,
            u"base=image:docker.io/library/debian:trixie\nimage=docker.io/library/debian:trixie\ndisplay=floating\n"_s);
        put(u"home/.config/kdos/boxes/app.later.conf"_s, u"base=pack:app.later\n"_s);
    }

    void module(const QVariantList &args = {})
    {
        m_kcm = std::make_unique<Kdos::BoxesModule>(nullptr, KPluginMetaData(), args);
        m_kcm->load();
        QTRY_VERIFY(m_kcm->boxes()->isLoaded());
    }

    void open(const QString &box)
    {
        QVERIFY(m_kcm->openBox(box));
        QTRY_VERIFY(m_kcm->profile()->notesLoaded());
        QTRY_VERIFY(m_kcm->current().value(u"listed"_s).toBool());
    }

    QVariant row(const QString &box, int role) const
    {
        const int i = m_kcm->boxes()->indexOf(box);
        return i < 0 ? QVariant() : m_kcm->boxes()->data(m_kcm->boxes()->index(i), role);
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_path = qgetenv("PATH");
    }

    void init()
    {
        machine();
    }

    void cleanup()
    {
        m_kcm.reset();
        m_root.reset();
        qputenv("PATH", m_path);
    }

    void listsContainersAndProfiles()
    {
        module();
        QTRY_COMPARE(m_kcm->boxes()->rowCount(), 3);
        QCOMPARE(row(u"app.gimp"_s, Kdos::BoxListModel::StateRole).toString(), u"running"_s);
        QCOMPARE(row(u"app.gimp"_s, Kdos::BoxListModel::AccentRole).toString(), u"amber"_s);
        QCOMPARE(row(u"app.gimp"_s, Kdos::BoxListModel::BaseRole).toString(), u"pack:app.gimp"_s);
        QCOMPARE(row(u"app.gimp"_s, Kdos::BoxListModel::ExportIdRole).toString(), u"app.gimp"_s);
        QVERIFY(row(u"app.gimp"_s, Kdos::BoxListModel::RunningRole).toBool());
        QCOMPARE(row(u"dev"_s, Kdos::BoxListModel::StateTextRole).toString(), u"Stopped"_s);
        QVERIFY(row(u"dev"_s, Kdos::BoxListModel::CreatedRole).toBool());
        QCOMPARE(row(u"dev"_s, Kdos::BoxListModel::ExportIdRole).toString(), QString());
        QCOMPARE(row(u"app.later"_s, Kdos::BoxListModel::StateRole).toString(), u"not created"_s);
        QVERIFY(!row(u"app.later"_s, Kdos::BoxListModel::CreatedRole).toBool());
    }

    void confinementIsReadForRunningBoxesOnly()
    {
        put(u"run/kdos/boxes/app.gimp.state"_s,
            u"# kdos-appbox: which boundary held for this box's last launch\n"
            "confinement = unconfined\nlauncher    = app.gimp\nsocket      = refused\n"
            "reason      = kdos-boxsock did not answer within 1 s\ntime        = 1790000000\n"_s);
        put(u"run/kdos/boxes/dev.state"_s, u"confinement = confined\n"_s);
        module();
        QTRY_COMPARE(row(u"app.gimp"_s, Kdos::BoxListModel::ConfinementRole).toString(), u"unconfined"_s);
        QCOMPARE(row(u"app.gimp"_s, Kdos::BoxListModel::ConfinementTextRole).toString(), u"Not confined"_s);
        QCOMPARE(row(u"app.gimp"_s, Kdos::BoxListModel::ConfinementReasonRole).toString(), u"kdos-boxsock did not answer within 1 s"_s);
        QCOMPARE(row(u"dev"_s, Kdos::BoxListModel::ConfinementRole).toString(), QString());
        QVERIFY(row(u"app.gimp"_s, Kdos::BoxListModel::SubtitleRole).toString().contains(u"Not confined"_s));

        /* The page's own view of the box carries the reason too. */
        open(u"app.gimp"_s);
        QCOMPARE(m_kcm->current().value(u"confinement"_s).toString(), u"unconfined"_s);
        QCOMPARE(m_kcm->current().value(u"confinementReason"_s).toString(), u"kdos-boxsock did not answer within 1 s"_s);

        /* A confined box has nothing to explain, whatever the file says. */
        put(u"run/kdos/boxes/app.gimp.state"_s, u"confinement = confined\nreason = stale\n"_s);
        m_kcm->boxes()->refresh();
        QTRY_COMPARE(row(u"app.gimp"_s, Kdos::BoxListModel::ConfinementRole).toString(), u"confined"_s);
        QCOMPARE(row(u"app.gimp"_s, Kdos::BoxListModel::ConfinementReasonRole).toString(), QString());
    }

    void confinementRecord()
    {
        const Kdos::ConfinementRecord rec = Kdos::parseConfinementRecord(
            u"# kdos-appbox\nconfinement = tagged\nlauncher    = app.maxima.wxmaxima\nruntime     = session\n"
            "bus         = unfiltered\nreason      = the box shares the session's runtime folder\n  and more\n"
            "time        = 1790000000\n"_s);
        QCOMPARE(rec.state, Kdos::Confinement::Tagged);
        QCOMPARE(rec.launcher, u"app.maxima.wxmaxima"_s);
        QCOMPARE(rec.reason, u"the box shares the session's runtime folder"_s);
        QCOMPARE(rec.time.toSecsSinceEpoch(), 1790000000);

        /* The first verdict wins, and a reason is one line of bounded
         * length whatever the file holds. */
        const Kdos::ConfinementRecord two = Kdos::parseConfinementRecord(u"confinement = unconfined\nstate = confined\nreason = "_s + QString(500, u'x'));
        QCOMPARE(two.state, Kdos::Confinement::Unconfined);
        QCOMPARE(two.reason.size(), 300);
        QVERIFY(!Kdos::parseConfinementRecord(u"time = soon\n"_s).time.isValid());
    }

    void parseConfinement_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("id");
        QTest::newRow("key") << u"confinement = confined\n"_s << u"confined"_s;
        QTest::newRow("state key") << u"pid = 4\nstate = tagged\n"_s << u"tagged"_s;
        QTest::newRow("bare word") << u"unconfined\n"_s << u"unconfined"_s;
        QTest::newRow("private alias") << u"confinement = private\n"_s << u"confined"_s;
        QTest::newRow("nothing") << QString() << QString();
        QTest::newRow("unknown word") << u"confinement = maybe\n"_s << QString();
    }

    void parseConfinement()
    {
        QFETCH(QString, text);
        QFETCH(QString, id);
        QCOMPARE(Kdos::confinementId(Kdos::parseConfinement(text)), id);
    }

    void parseListing()
    {
        const auto rows = Kdos::BoxListModel::parseListing(
            u"[{\"name\":\"app.x\",\"base\":\"pack:app.x\",\"state\":\"running\",\"persistence\":\"persistent\",\"accent\":\"ice\",\"disk\":1024,\"created\":1790000000},"
            "{\"name\":\"later\",\"base\":\"\",\"state\":\"not created\",\"persistence\":\"ephemeral\",\"accent\":\"session\",\"disk\":0,\"created\":0},"
            "{\"name\":\"../evil\",\"state\":\"running\"}]"_s);
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0).accent, u"ice"_s);
        QCOMPARE(rows.at(0).created.toSecsSinceEpoch(), 1790000000);
        QCOMPARE(rows.at(0).diskBytes, 1024);
        QVERIFY(!rows.at(0).diskText().isEmpty());
        QCOMPARE(rows.at(1).state, u"not created"_s);
        QVERIFY(!rows.at(1).hasContainer());
        QVERIFY(!rows.at(1).created.isValid());
        QVERIFY(rows.at(1).diskText().isEmpty());
        /* The table is for a person and reads as no boxes at all. */
        QVERIFY(Kdos::BoxListModel::parseListing(u"BOX  BASE  STATE\napp.a  pack:app.a  running\n"_s).isEmpty());
    }

    void profileIsReadTheWayKdosBoxReadsIt()
    {
        put(u"home/.config/kdos/boxes/dev.conf"_s,
            u"base=image:docker.io/library/debian:trixie\n"
            "network = yes\nhome = 1\ngpu = shared\nrender = pixman\nmemory = 1.5G\ncpus = 2.5\n"
            "autostop = 30m\ngrant = layer-shell window-list,clipboard-history\n"_s);
        module();
        open(u"dev"_s);
        Kdos::BoxProfile *p = m_kcm->profile();
        QCOMPARE(p->values()->value(u"network"_s).toString(), u"private"_s);
        QCOMPARE(p->values()->value(u"home"_s).toString(), u"private"_s);
        QCOMPARE(p->values()->value(u"gpu"_s).toString(), u"yes"_s);
        QCOMPARE(p->values()->value(u"render"_s).toString(), u"software"_s);
        QCOMPARE(p->values()->value(u"memory"_s).toInt(), 1536);
        QCOMPARE(p->values()->value(u"cpus"_s).toInt(), 25);
        QCOMPARE(p->values()->value(u"autostop"_s).toInt(), 1800);
        QCOMPARE(p->values()->value(u"grant"_s).toStringList(), (QStringList{u"layer-shell"_s, u"window-list"_s, u"clipboard-history"_s}));
        QCOMPARE(p->otherGrants(), QStringList{u"clipboard-history"_s});
        QVERIFY(!p->isDirty());
        QVERIFY(!m_kcm->needsSave());
    }

    /* The report's layout, read from a fixed text: every `!` line under the
     * key above it, the two-key note under both, the rest at the top. */
    void reportLinesLandUnderTheirKeys()
    {
        Kdos::BoxProfile p;
        p.applyReport(
            u"box         = dev\n"
            "base        = image:docker.io/library/debian:trixie\n"
            "accent      = ice\n"
            "persistence = ephemeral   recorded in the profile\n"
            "            ! ephemeral is not enforced\n"
            "network     = host        --network host\n"
            "wayland     = no          the box's own tagged socket only\n"
            "            ! wayland=no cannot be enforced on a box created over the session's runtime folder\n"
            "x11         = no          no cookie in its runtime folder\n"
            "gpu         = yes         follows `devices`\n"
            "            ! audio=no gpu=yes cannot be enforced separately\n"
            "home-path   = /home/u/.local/share/kdos/boxes/dev\n"
            "            ! about a key the page does not show\n"
            "            ! boxes/dev.conf: unknown key 'bogus'\n"_s);
        QVERIFY(p.notesLoaded());
        QCOMPARE(p.notes()->value(u"persistence"_s).toStringList(), QStringList{u"ephemeral is not enforced"_s});
        QCOMPARE(p.notes()->value(u"wayland"_s).toStringList().size(), 1);
        QVERIFY(p.notes()->value(u"network"_s).toStringList().isEmpty());
        QVERIFY(p.notes()->value(u"gpu"_s).toStringList().join(u' ').contains(u"audio=no"_s));
        QVERIFY(p.notes()->value(u"audio"_s).toStringList().join(u' ').contains(u"audio=no"_s));
        QCOMPARE(p.generalNotes(), (QStringList{u"about a key the page does not show"_s, u"boxes/dev.conf: unknown key 'bogus'"_s}));
        QCOMPARE(p.mechanisms()->value(u"network"_s).toString(), u"--network host"_s);
        QCOMPARE(p.mechanisms()->value(u"x11"_s).toString(), u"no cookie in its runtime folder"_s);
    }

    /* The same rule against what the real kdos-box prints, whatever its
     * wording: each `!` line is under the control of the key above it. */
    void eachNoteSitsUnderItsControl()
    {
        put(u"home/.config/kdos/boxes/dev.conf"_s,
            u"base=image:docker.io/library/debian:trixie\nwayland=no\npersistence=ephemeral\naudio=no\nbogus=1\n"_s);
        module();
        open(u"dev"_s);
        Kdos::BoxProfile *p = m_kcm->profile();

        const QString rep = report(u"dev"_s);
        static const QRegularExpression keyLine(u"^([a-z][a-z0-9-]*)\\s*="_s);
        static const QRegularExpression noteLine(u"^\\s+!\\s*(.*)$"_s);
        QString key;
        int notes = 0;
        for (const QString &line : rep.split(u'\n')) {
            const auto k = keyLine.match(line);
            if (k.hasMatch()) {
                key = k.captured(1);
                continue;
            }
            const auto n = noteLine.match(line);
            if (!n.hasMatch()) {
                continue;
            }
            const QString text = n.captured(1).trimmed();
            ++notes;
            if (Kdos::BoxProfile::editableKeys().contains(key) && !text.contains(u"unknown key"_s)) {
                QVERIFY2(p->notes()->value(key).toStringList().contains(text), qPrintable(key + u": "_s + text));
            } else {
                QVERIFY2(p->generalNotes().contains(text), qPrintable(text));
            }
        }
        QVERIFY2(notes > 0, qPrintable(rep));
        QVERIFY2(p->generalNotes().join(u' ').contains(u"unknown key 'bogus'"_s), qPrintable(p->generalNotes().join(u'|')));
        QVERIFY(!p->mechanisms()->value(u"network"_s).toString().isEmpty());
        QVERIFY(!p->mechanisms()->value(u"ipc"_s).toString().isEmpty());
    }

    /* One check box per grant name, in the order the profile spells them,
     * each with a label and what it lets the box do. */
    void grantsCoverTheVocabulary()
    {
        module();
        QStringList ids;
        for (const QVariant &g : m_kcm->grants()) {
            const QVariantMap m = g.toMap();
            ids << m.value(u"id"_s).toString();
            QVERIFY(!m.value(u"label"_s).toString().isEmpty());
            QVERIFY(!m.value(u"description"_s).toString().isEmpty());
        }
        QCOMPARE(ids, Kdos::BoxProfile::grantVocabulary());
        QCOMPARE(ids.size(), 10);
        QVERIFY(ids.contains(u"input-method"_s));
    }

    void argumentsNameABoxOrNothing()
    {
        QCOMPARE(Kdos::BoxesModule::boxFromArgs({QVariant(u"app.gimp"_s)}), u"app.gimp"_s);
        QCOMPARE(Kdos::BoxesModule::boxFromArgs({QVariant(u"--evil"_s)}), QString());
        QCOMPARE(Kdos::BoxesModule::boxFromArgs({}), QString());

        /* A System Settings already showing the page asks again. */
        module();
        QSignalSpy spy(m_kcm.get(), &Kdos::BoxesModule::boxRequested);
        Q_EMIT m_kcm->activationRequested({QVariant(u"dev"_s)});
        Q_EMIT m_kcm->activationRequested({QVariant(u"../dev"_s)});
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), u"dev"_s);
    }

    /* The 6.4 gate: `kdos-box profile` reflects every control. */
    void everyControlReachesKdosBox()
    {
        module();
        open(u"dev"_s);
        QQmlPropertyMap *v = m_kcm->profile()->values();
        v->insert(u"accent"_s, u"ice"_s);
        v->insert(u"grant"_s, QStringList{u"layer-shell"_s, u"window-list"_s});
        v->insert(u"persistence"_s, u"frozen"_s);
        v->insert(u"network"_s, u"none"_s);
        v->insert(u"ipc"_s, u"private"_s);
        v->insert(u"devices"_s, u"private"_s);
        v->insert(u"processes"_s, u"private"_s);
        v->insert(u"home"_s, u"private"_s);
        v->insert(u"init"_s, u"yes"_s);
        v->insert(u"wayland"_s, u"no"_s);
        v->insert(u"audio"_s, u"no"_s);
        v->insert(u"gpu"_s, u"no"_s);
        v->insert(u"render"_s, u"software"_s);
        v->insert(u"memory"_s, 4096);
        v->insert(u"cpus"_s, 15);
        v->insert(u"pids"_s, 512);
        v->insert(u"autostop"_s, 600);
        v->insert(u"x11"_s, u"no"_s);
        /* Every key the page edits is a change. */
        QCOMPARE(m_kcm->profile()->changedKeys(), Kdos::BoxProfile::editableKeys());

        m_kcm->save();
        QTRY_COMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Positive));

        const QString file = profileFile(u"dev"_s);
        QVERIFY2(file.contains(u"\nx11=no\n"_s), qPrintable(file));
        for (const QString &line : {u"accent=ice"_s,
                                    u"grant=window-list,layer-shell"_s,
                                    u"persistence=frozen"_s,
                                    u"network=none"_s,
                                    u"ipc=private"_s,
                                    u"devices=private"_s,
                                    u"processes=private"_s,
                                    u"home=private"_s,
                                    u"init=yes"_s,
                                    u"wayland=no"_s,
                                    u"audio=no"_s,
                                    u"gpu=no"_s,
                                    u"render=software"_s,
                                    u"memory=4G"_s,
                                    u"cpus=1.5"_s,
                                    u"pids=512"_s,
                                    u"autostop=600s"_s}) {
            QVERIFY2(file.contains(u'\n' + line + u'\n'), qPrintable(line + u" not in:\n"_s + file));
        }
        /* Keys the page does not show keep what they held. */
        QVERIFY(file.contains(u"\ndisplay=floating\n"_s));
        QVERIFY(file.contains(u"\nimage=docker.io/library/debian:trixie\n"_s));

        QString rep = report(u"dev"_s);
        QVERIFY2(rep.contains(u"x11         = no"_s), qPrintable(rep));
        for (const QString &line : {u"network     = none"_s,
                                    u"ipc         = private"_s,
                                    u"processes   = private"_s,
                                    u"home        = private"_s,
                                    u"persistence = frozen"_s,
                                    u"memory      = 4G"_s,
                                    u"cpus        = 1.5"_s,
                                    u"pids        = 512"_s,
                                    u"autostop    = 600"_s,
                                    u"accent      = ice"_s,
                                    u"grant       = window-list,layer-shell"_s,
                                    u"render      = software"_s,
                                    u"gpu         = no"_s}) {
            QVERIFY2(rep.contains(line), qPrintable(line + u" not in:\n"_s + rep));
        }

        /* The page reloads what was written and has nothing left to save. */
        QTRY_VERIFY(!m_kcm->needsSave());
        QCOMPARE(m_kcm->profile()->values()->value(u"memory"_s).toInt(), 4096);
    }

    /* The 6.4 gate: changing network to `none` shows the recreate banner. */
    void createTimeChangeShowsTheRecreateBanner()
    {
        module();
        open(u"dev"_s);
        QVERIFY(!m_kcm->needsRecreate());
        m_kcm->profile()->values()->insert(u"network"_s, u"none"_s);
        m_kcm->save();
        QTRY_VERIFY(m_kcm->needsRecreate());
        QCOMPARE(m_kcm->profile()->pendingRecreate(), QStringList{u"network"_s});

        /* The page's memory outlives the page. */
        m_kcm.reset();
        module();
        open(u"dev"_s);
        QVERIFY(m_kcm->needsRecreate());

        /* A colour is not a create-time key. */
        open(u"app.gimp"_s);
        m_kcm->profile()->values()->insert(u"accent"_s, u"ice"_s);
        m_kcm->save();
        QTRY_COMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Positive));
        QVERIFY(!m_kcm->needsRecreate());
    }

    void boxWithoutContainerNeedsNoRecreate()
    {
        module();
        open(u"app.later"_s);
        m_kcm->profile()->values()->insert(u"network"_s, u"none"_s);
        m_kcm->save();
        QTRY_COMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Positive));
        QVERIFY(m_kcm->profile()->pendingRecreate().isEmpty());
        QVERIFY(!m_kcm->needsRecreate());
        QVERIFY(profileFile(u"app.later"_s).contains(u"\nnetwork=none\n"_s));
    }

    void recreateStopsRemovesAndCreates()
    {
        put(u"fake/podman.state"_s, u"container dev running\nimage docker.io/library/debian:trixie\n"_s);
        module();
        open(u"dev"_s);
        m_kcm->profile()->values()->insert(u"network"_s, u"none"_s);
        m_kcm->save();
        QTRY_VERIFY(m_kcm->needsRecreate());

        m_kcm->recreateBox();
        QTRY_VERIFY_WITH_TIMEOUT(!m_kcm->isBusy(), 20000);
        QCOMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Positive));

        const QString log = calls();
        const qsizetype stop = log.indexOf(u"podman kill --signal TERM dev"_s);
        const qsizetype rm = log.indexOf(u"podman rm dev"_s);
        const qsizetype create = log.indexOf(u"distrobox create --name dev"_s);
        QVERIFY2(stop >= 0 && rm > stop && create > rm, qPrintable(log));
        const QString createLine = log.mid(create, log.indexOf(u'\n', create) - create);
        QVERIFY2(createLine.contains(u"--unshare-netns"_s) && createLine.contains(u"--network none"_s), qPrintable(createLine));
        QVERIFY(get(u"fake/podman.state"_s).contains(u"container dev created"_s));
        QTRY_VERIFY(!m_kcm->needsRecreate());
        QVERIFY(m_kcm->profile()->pendingRecreate().isEmpty());
    }

    void recreateThatCannotCreateSaysSo()
    {
        /* A pack box is created through kdos-packd, which is not running
         * here: the container goes, the profile stays, and the page says
         * which half happened. */
        module();
        open(u"app.gimp"_s);
        m_kcm->recreateBox();
        QTRY_VERIFY_WITH_TIMEOUT(!m_kcm->isBusy(), 20000);
        QCOMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Warning));
        QVERIFY2(m_kcm->message().contains(u"creating it again failed"_s), qPrintable(m_kcm->message()));
        QVERIFY(!get(u"fake/podman.state"_s).contains(u"app.gimp"_s));
        QVERIFY(profileFile(u"app.gimp"_s).contains(u"base=pack:app.gimp"_s));
    }

    void removeKeepsTheProfile()
    {
        module();
        open(u"dev"_s);
        m_kcm->removeBox();
        QTRY_VERIFY_WITH_TIMEOUT(!m_kcm->isBusy(), 20000);
        QCOMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Positive));
        QVERIFY(!get(u"fake/podman.state"_s).contains(u"container dev"_s));
        QVERIFY(!profileFile(u"dev"_s).isEmpty());
        QTRY_COMPARE(row(u"dev"_s, Kdos::BoxListModel::StateRole).toString(), u"not created"_s);
    }

    void removingAnUncreatedBoxDeletesItsSettings()
    {
        module();
        open(u"app.later"_s);
        m_kcm->removeBox();
        QTRY_VERIFY_WITH_TIMEOUT(!m_kcm->isBusy(), 20000);
        QVERIFY(!QFile::exists(root(u"home/.config/kdos/boxes/app.later.conf"_s)));
        QCOMPARE(m_kcm->currentBox(), QString());
        QTRY_COMPARE(m_kcm->boxes()->indexOf(u"app.later"_s), -1);
    }

    /* kdos-box's words reach the page as prose: quoted words and settings
     * as code, a sentence's first word capitalised, nothing else changed. */
    void notesReadAsProse_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("sentence");
        QTest::addColumn<QString>("markup");
        QTest::newRow("quoted word") << u"the limits are podman flags — kdos-oomd holds it to `memory`"_s << true
                                     << u"<span>The limits are podman flags — kdos-oomd holds it to <code>memory</code></span>"_s;
        QTest::newRow("setting first") << u"audio=no gpu=yes cannot be enforced."_s << true
                                       << u"<span><code>audio=no</code> <code>gpu=yes</code> cannot be enforced.</span>"_s;
        QTest::newRow("path first") << u"boxes/dev.conf: unknown key 'bogus'"_s << true << u"<span>boxes/dev.conf: unknown key 'bogus'</span>"_s;
        QTest::newRow("escaped") << u"a <b> & c"_s << true << u"<span>A &lt;b&gt; &amp; c</span>"_s;
        QTest::newRow("mechanism") << u"enforced by `kdos-box gc`"_s << false << u"<span>enforced by <code>kdos-box gc</code></span>"_s;
    }

    void notesReadAsProse()
    {
        QFETCH(QString, text);
        QFETCH(bool, sentence);
        QFETCH(QString, markup);
        QCOMPARE(Kdos::BoxProfile::markup(text, sentence), markup);
    }

    /* A command that fails and prints nothing gets a sentence with no
     * colon, and a listing that runs during it never becomes its progress
     * line. */
    void silentFailureHasNoEmptyReason()
    {
        module();
        open(u"dev"_s);
        /* kdos-box itself, except that `start` fails without a word. */
        const QString tool = root(u"bin/kdos-box"_s);
        QVERIFY(QDir().mkpath(root(u"real"_s)));
        QVERIFY(QFile::link(QStringLiteral(KDOS_APPBOX_PATH), root(u"real/kdos-box"_s)));
        QVERIFY(QFile::remove(tool));
        put(u"bin/kdos-box"_s, u"#!/bin/sh\n[ \"$1\" = start ] && { sleep 1; exit 1; }\nexec \"%1\" \"$@\"\n"_s.arg(root(u"real/kdos-box"_s)));
        QVERIFY(QFile::setPermissions(tool, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        QStringList progress;
        const auto watch = connect(m_kcm.get(), &Kdos::BoxesModule::progressTextChanged, this, [this, &progress] {
            progress << m_kcm->progressText();
        });
        m_kcm->startBox();
        m_kcm->boxes()->refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!m_kcm->isBusy(), 20000);
        QCOMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Error));
        disconnect(watch);
        QCOMPARE(m_kcm->message(), u"dev did not start."_s);
        for (const QString &line : std::as_const(progress)) {
            QVERIFY2(!line.startsWith(u'[') && !line.startsWith(u'{'), qPrintable(line));
        }
    }

    void startAndStop()
    {
        module();
        open(u"dev"_s);
        m_kcm->startBox();
        QTRY_VERIFY_WITH_TIMEOUT(!m_kcm->isBusy(), 20000);
        QVERIFY(calls().contains(u"podman start dev"_s));
        QTRY_VERIFY(m_kcm->current().value(u"running"_s).toBool());
        m_kcm->stopBox();
        QTRY_VERIFY_WITH_TIMEOUT(!m_kcm->isBusy(), 20000);
        QTRY_VERIFY(!m_kcm->current().value(u"running"_s).toBool());
    }

    void exportNamesTheCatalogueId()
    {
        module();
        open(u"app.gimp"_s);
        m_kcm->exportBox(QUrl::fromLocalFile(root(u"out/gimp"_s)));
        QTRY_VERIFY_WITH_TIMEOUT(!m_kcm->isBusy(), 20000);
        QCOMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Positive));
        QVERIFY2(calls().contains(u"kdos-appbox export "_s + root(u"out/gimp.ktar"_s) + u" app.gimp\n"_s), qPrintable(calls()));

        /* A box that is not an application box has no archive. */
        open(u"dev"_s);
        m_kcm->exportBox(QUrl::fromLocalFile(root(u"out/dev.ktar"_s)));
        QCOMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Error));
    }

    void noActWithUnappliedEdits()
    {
        module();
        open(u"dev"_s);
        /* A QML write is what marks the page dirty, and it arrives as
         * valueChanged; insert() from C++ alone is a load. */
        m_kcm->profile()->values()->insert(u"ipc"_s, u"private"_s);
        Q_EMIT m_kcm->profile()->values()->valueChanged(u"ipc"_s, u"private"_s);
        QVERIFY(m_kcm->needsSave());
        m_kcm->startBox();
        QCOMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Warning));
        QTest::qWait(300);
        QVERIFY(!calls().contains(u"podman start"_s));
    }

    void namesAreCheckedBeforeAnythingRuns()
    {
        module({QVariant(u"../../etc/passwd"_s)});
        QCOMPARE(m_kcm->initialBox(), QString());
        QVERIFY(!m_kcm->openBox(u"-rf"_s));
        QVERIFY(!m_kcm->openBox(u"a b"_s));
        QVERIFY(!m_kcm->openBox(QString()));
        QCOMPARE(m_kcm->messageType(), int(Kdos::BoxesModule::Error));
        m_kcm.reset();
        module({QVariant(u"app.gimp"_s)});
        QCOMPARE(m_kcm->initialBox(), u"app.gimp"_s);
        QVERIFY(Kdos::BoxTool::isValidBoxName(u"app.maxima.wxmaxima"_s));
        QVERIFY(!Kdos::BoxTool::isValidBoxName(QString(64, u'a')));
        QVERIFY(!Kdos::BoxTool::isValidBoxName(u".hidden"_s));
    }

    void defaultsKeepGrantsThePageDoesNotShow()
    {
        put(u"home/.config/kdos/boxes/dev.conf"_s, u"base=image:docker.io/library/debian:trixie\nnetwork=none\ngrant=window-list,clipboard-history\n"_s);
        module();
        open(u"dev"_s);
        QVERIFY(!m_kcm->profile()->isAtDefaults());
        m_kcm->defaults();
        QCOMPARE(m_kcm->profile()->values()->value(u"network"_s).toString(), u"host"_s);
        QCOMPARE(m_kcm->profile()->values()->value(u"grant"_s).toStringList(), QStringList{u"clipboard-history"_s});
        QVERIFY(m_kcm->profile()->isDirty());
    }

    void conversions_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("raw");
        QTest::addColumn<QVariant>("value");
        QTest::addColumn<QString>("spelled");
        QTest::newRow("memory G") << u"memory"_s << u"4G"_s << QVariant(4096) << u"4G"_s;
        QTest::newRow("memory M") << u"memory"_s << u"1536m"_s << QVariant(1536) << u"1536M"_s;
        QTest::newRow("memory GiB") << u"memory"_s << u"2 GiB"_s << QVariant(2048) << u"2G"_s;
        QTest::newRow("memory bytes") << u"memory"_s << u"1073741824"_s << QVariant(1024) << u"1G"_s;
        QTest::newRow("memory none") << u"memory"_s << QString() << QVariant(0) << QString();
        QTest::newRow("x11 default") << u"x11"_s << QString() << QVariant(u"auto"_s) << u"auto"_s;
        QTest::newRow("x11 yes") << u"x11"_s << u"on"_s << QVariant(u"yes"_s) << u"yes"_s;
        QTest::newRow("x11 off") << u"x11"_s << u"off"_s << QVariant(u"no"_s) << u"no"_s;
        QTest::newRow("x11 never") << u"x11"_s << u"never"_s << QVariant(u"auto"_s) << u"auto"_s;
        QTest::newRow("x11 typo") << u"x11"_s << u"nope"_s << QVariant(u"auto"_s) << u"auto"_s;
        QTest::newRow("grant order") << u"grant"_s << u"secrets layer-shell,odd"_s << QVariant(QStringList{u"secrets"_s, u"layer-shell"_s, u"odd"_s})
                                     << u"layer-shell,secrets,odd"_s;
        QTest::newRow("cpus") << u"cpus"_s << u"2"_s << QVariant(20) << u"2"_s;
        QTest::newRow("cpus fraction") << u"cpus"_s << u"0.5"_s << QVariant(5) << u"0.5"_s;
        QTest::newRow("autostop h") << u"autostop"_s << u"2h"_s << QVariant(7200) << u"7200s"_s;
        QTest::newRow("autostop s") << u"autostop"_s << u"90s"_s << QVariant(90) << u"90s"_s;
        QTest::newRow("autostop off") << u"autostop"_s << u"0s"_s << QVariant(0) << u"0"_s;
        QTest::newRow("network none") << u"network"_s << u"none"_s << QVariant(u"none"_s) << u"none"_s;
        QTest::newRow("network shared") << u"network"_s << u"shared"_s << QVariant(u"host"_s) << u"host"_s;
        QTest::newRow("render off") << u"render"_s << u"off"_s << QVariant(u"software"_s) << u"software"_s;
        QTest::newRow("persistence typo") << u"persistence"_s << u"ephemral"_s << QVariant(u"persistent"_s) << u"persistent"_s;
        QTest::newRow("accent empty") << u"accent"_s << QString() << QVariant(u"session"_s) << u"session"_s;
    }

    void conversions()
    {
        QFETCH(QString, key);
        QFETCH(QString, raw);
        QFETCH(QVariant, value);
        QFETCH(QString, spelled);
        const QVariant v = Kdos::BoxProfile::normalize(key, raw);
        if (value.typeId() == QMetaType::QStringList) {
            QCOMPARE(v.toStringList(), value.toStringList());
        } else {
            QCOMPARE(v.toString(), value.toString());
        }
        QCOMPARE(Kdos::BoxProfile::spell(key, v), spelled);
    }

    void accentsCarryTheirSchemesColours()
    {
        put(u"home/.local/share/color-schemes/KDOSAmber.colors"_s,
            u"[General]\nName=KDOS Amber\n[Colors:Header]\nBackgroundNormal=74,47,0\nForegroundNormal=255,176,0\nDecorationFocus=255,200,0\n"_s);
        put(u"home/.local/share/color-schemes/KDOSIce.colors"_s,
            u"[General]\nName=KDOS Ice\n[WM]\nactiveBackground=18,58,74\nactiveForeground=191,238,255\nactiveBlend=77,215,255\n"_s);
        Kdos::AccentModel model;
        QCOMPARE(model.rowCount(), 9);
        QCOMPARE(model.data(model.index(0), Kdos::AccentModel::IdRole).toString(), u"session"_s);
        const QModelIndex amber = model.index(model.indexOf(u"amber"_s));
        QVERIFY(model.data(amber, Kdos::AccentModel::AvailableRole).toBool());
        QCOMPARE(model.data(amber, Kdos::AccentModel::ColorRole).value<QColor>(), QColor(74, 47, 0));
        QCOMPARE(model.data(amber, Kdos::AccentModel::TextColorRole).value<QColor>(), QColor(255, 176, 0));
        const QModelIndex ice = model.index(model.indexOf(u"ice"_s));
        QCOMPARE(model.data(ice, Kdos::AccentModel::ColorRole).value<QColor>(), QColor(18, 58, 74));
        QCOMPARE(model.data(amber, Kdos::AccentModel::AccentColorRole).value<QColor>(), QColor(255, 200, 0));
        QCOMPARE(model.data(ice, Kdos::AccentModel::AccentColorRole).value<QColor>(), QColor(77, 215, 255));
        const QModelIndex paper = model.index(model.indexOf(u"paper"_s));
        QVERIFY(!model.data(paper, Kdos::AccentModel::AvailableRole).toBool());
        QCOMPARE(Kdos::AccentModel::schemeName(u"session"_s), u"KDOSBox"_s);
        QCOMPARE(Kdos::AccentModel::schemeName(u"borland"_s), u"KDOSBorland"_s);
    }
};

QTEST_GUILESS_MAIN(BoxesTest)

#include "boxestest.moc"
