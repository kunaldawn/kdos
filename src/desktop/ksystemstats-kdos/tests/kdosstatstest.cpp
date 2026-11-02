/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosstatstest — the readings, against trees and replies built here
 *
 * Every cgroup tree is made in a temporary directory, so a scope name with a
 * `\x2d` in it never has to live in the repository.
 */

#include "kdosstats.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace Kdos;

namespace
{
void put(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

void scope(const QString &dir, const QByteArray &usec, const QByteArray &mem, const QByteArray &max, const QByteArray &io, const QByteArray &pids)
{
    QDir().mkpath(dir);
    put(dir + QStringLiteral("/cpu.stat"), "usage_usec " + usec + "\nuser_usec 1\nsystem_usec 2\n");
    put(dir + QStringLiteral("/memory.current"), mem + "\n");
    put(dir + QStringLiteral("/memory.max"), max + "\n");
    put(dir + QStringLiteral("/io.stat"), io);
    put(dir + QStringLiteral("/pids.current"), pids + "\n");
}
} // namespace

class KdosStatsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void scopeNames_data()
    {
        QTest::addColumn<QString>("scope");
        QTest::addColumn<QString>("box");
        QTest::newRow("plain") << QStringLiteral("app-kdos-www-1730000000.scope") << QStringLiteral("www");
        QTest::newRow("dotted") << QStringLiteral("app-kdos-app.firefox-17.scope") << QStringLiteral("app.firefox");
        QTest::newRow("escaped dash") << QStringLiteral("app-kdos-my\\x2dbox-5.scope") << QStringLiteral("my-box");
        QTest::newRow("no number") << QStringLiteral("app-kdos-www.scope") << QString();
        QTest::newRow("number not digits") << QStringLiteral("app-kdos-www-12a.scope") << QString();
        QTest::newRow("empty box") << QStringLiteral("app-kdos--12.scope") << QString();
        QTest::newRow("other app scope") << QStringLiteral("app-org.kde.konsole-3.scope") << QString();
        QTest::newRow("service") << QStringLiteral("app-kdos-www-1.service") << QString();
        QTest::newRow("escaped slash") << QStringLiteral("app-kdos-a\\x2fb-1.scope") << QString();
        QTest::newRow("bad escape kept") << QStringLiteral("app-kdos-a\\xZZ-1.scope") << QStringLiteral("a\\xZZ");
    }

    void scopeNames()
    {
        QFETCH(QString, scope);
        QFETCH(QString, box);
        QCOMPARE(kdosBoxFromScope(scope), box);
    }

    void findsScopesInBothPlaces()
    {
        QTemporaryDir t;
        QVERIFY(t.isValid());
        const QString r = t.path();
        const QString u = QString::number(::getuid());
        QDir().mkpath(r + QStringLiteral("/c2/delegated/app-kdos-www-100.scope/libpod-abc"));
        QDir().mkpath(r + QStringLiteral("/c2/delegated/app-kdos-mail-7.scope"));
        QDir().mkpath(r + QStringLiteral("/c2/session"));
        QDir().mkpath(r + QStringLiteral("/c3/delegated/app-kdos-www-200.scope"));
        QDir().mkpath(r + QStringLiteral("/c3/delegated/not-a-box"));
        QDir().mkpath(r + QStringLiteral("/user.slice/user-%1.slice/user@%1.service/app.slice/app-kdos-office-9.scope").arg(u));
        QDir().mkpath(r + QStringLiteral("/user.slice/user-%1.slice/user@%1.service/app.slice/app-org.kde.dolphin-1.scope").arg(u));

        const QList<BoxScope> s = kdosFindBoxScopes(r, ::getuid());
        QCOMPARE(s.size(), 4);
        QCOMPARE(s[0].box, QStringLiteral("mail"));
        QCOMPARE(s[1].box, QStringLiteral("office"));
        QCOMPARE(s[2].box, QStringLiteral("www"));
        QVERIFY(s[2].path.endsWith(QStringLiteral("/c2/delegated/app-kdos-www-100.scope")));
        QCOMPARE(s[3].box, QStringLiteral("www"));
        QVERIFY(s[3].path.endsWith(QStringLiteral("/c3/delegated/app-kdos-www-200.scope")));
    }

    void otherUsersScopesAreNotListed()
    {
        QTemporaryDir t;
        QDir().mkpath(t.path() + QStringLiteral("/c2/delegated/app-kdos-www-1.scope"));
        QVERIFY(kdosFindBoxScopes(t.path(), ::getuid() + 1).isEmpty());
        QCOMPARE(kdosFindBoxScopes(t.path(), ::getuid()).size(), 1);
    }

    void missingRootIsEmpty()
    {
        QVERIFY(kdosFindBoxScopes(QStringLiteral("/nonexistent/kdos/cgroup"), ::getuid()).isEmpty());
    }

    void readsCounters()
    {
        QTemporaryDir t;
        const QString d = t.path() + QStringLiteral("/s");
        scope(d, "4500000", "104857600", "536870912",
              "8:0 rbytes=1000 wbytes=2000 rios=1 wios=2 dbytes=0 dios=0\n"
              "259:0 rbytes=24 wbytes=76 rios=1 wios=1 dbytes=0 dios=0\n",
              "12");
        const CgroupCounters c = kdosReadCgroup(d);
        QVERIFY(c.present);
        QCOMPARE(c.cpuUsec, 4500000);
        QCOMPARE(c.memoryBytes, 104857600);
        QCOMPARE(c.memoryMaxBytes, 536870912);
        QCOMPARE(c.readBytes, 1024);
        QCOMPARE(c.writeBytes, 2076);
        QCOMPARE(c.processes, 12);
    }

    void unlimitedAndMissingControllers()
    {
        QTemporaryDir t;
        const QString d = t.path() + QStringLiteral("/s");
        QDir().mkpath(d);
        put(d + QStringLiteral("/cpu.stat"), "usage_usec 10\n");
        put(d + QStringLiteral("/memory.max"), "max\n");
        const CgroupCounters c = kdosReadCgroup(d);
        QVERIFY(c.present);
        QCOMPARE(c.cpuUsec, 10);
        QCOMPARE(c.memoryMaxBytes, 0);
        QCOMPARE(c.memoryBytes, -1);
        QCOMPARE(c.readBytes, -1);
        QCOMPARE(c.processes, -1);

        QVERIFY(!kdosReadCgroup(t.path() + QStringLiteral("/gone")).present);
    }

    void garbageIsUnknown()
    {
        QTemporaryDir t;
        const QString d = t.path() + QStringLiteral("/s");
        scope(d, "-5", "lots", "-1", "8:0 rbytes=x wbytes=-3\n", "");
        const CgroupCounters c = kdosReadCgroup(d);
        QCOMPARE(c.cpuUsec, -1);
        QCOMPARE(c.memoryBytes, -1);
        QCOMPARE(c.memoryMaxBytes, -1);
        QCOMPARE(c.readBytes, 0);
        QCOMPARE(c.writeBytes, 0);
        QCOMPARE(c.processes, -1);
    }

    void addsTwoScopes()
    {
        CgroupCounters a, b;
        a.present = true;
        a.cpuUsec = 10;
        a.memoryBytes = 100;
        a.memoryMaxBytes = 1000;
        b.present = true;
        b.cpuUsec = 5;
        b.memoryMaxBytes = 0;
        b.processes = 3;
        const CgroupCounters c = kdosAddCounters(a, b);
        QCOMPARE(c.cpuUsec, 15);
        QCOMPARE(c.memoryBytes, 100);
        QCOMPARE(c.memoryMaxBytes, 0);
        QCOMPARE(c.processes, 3);
        QCOMPARE(c.readBytes, -1);
    }

    void launchers()
    {
        QTemporaryDir home, sys;
        put(home.path() + QStringLiteral("/app.maxima.wxmaxima.desktop"),
            "[Desktop Entry]\nName=wxMaxima\nIcon=app.maxima.wxmaxima\nX-KDOS-Box=app.maxima\n");
        put(home.path() + QStringLiteral("/app.maxima.desktop"), "[Desktop Entry]\nName=Maxima\nIcon=app.maxima\nX-KDOS-Box=app.maxima\n");
        put(home.path() + QStringLiteral("/www.desktop"),
            "# a comment\n[Desktop Entry]\nName=Firefox\nName[de]=Feuerfuchs\nIcon=firefox\nX-KDOS-Box=www\n[Desktop Action new]\nName=New Window\n");
        put(home.path() + QStringLiteral("/plain.desktop"), "[Desktop Entry]\nName=Plain\n");
        put(sys.path() + QStringLiteral("/www.desktop"), "[Desktop Entry]\nName=System Firefox\nX-KDOS-Box=www\n");
        put(sys.path() + QStringLiteral("/office.desktop"), "[Desktop Entry]\nName=LibreOffice\nX-KDOS-Box=office\n");

        const auto l = kdosBoxLaunchers({home.path(), sys.path()});
        QCOMPARE(l.size(), 3);
        QCOMPARE(l.value(QStringLiteral("app.maxima")).name, QStringLiteral("Maxima"));
        QCOMPARE(l.value(QStringLiteral("www")).name, QStringLiteral("Firefox"));
        QCOMPARE(l.value(QStringLiteral("www")).icon, QStringLiteral("firefox"));
        QCOMPARE(l.value(QStringLiteral("office")).name, QStringLiteral("LibreOffice"));
    }

    void energyReport()
    {
        const QByteArray reply =
            "{\"window_s\":7560.0,\"samples\":756,\"domains\":\"package-0 + psys\",\"gpu_in_domain\":true,"
            "\"gpu_accounting\":true,\"idle_floor_w\":3.250,\"floor_settled\":true,\"attributable_fraction\":0.6125,"
            "\"apps\":[{\"name\":\"firefox (appbox www)\",\"impact\":0.4120,\"cpu_s\":812.5,\"gpu_share\":0.2210,\"app\":\"firefox\",\"box\":\"www\"},"
            "{\"name\":\"kwin_wayland\",\"impact\":0.0940,\"cpu_s\":120.0,\"gpu_share\":0.6100,\"app\":\"kwin_wayland\",\"box\":null},"
            "{\"name\":\"a/b c\",\"impact\":0.01,\"cpu_s\":1.0,\"app\":\"a/b c\",\"box\":null},"
            "{\"name\":\"firefox (appbox www)\",\"impact\":0.001,\"cpu_s\":1.0,\"app\":\"firefox\",\"box\":\"www\"}],"
            "\"short_lived\":0.0310,\"suspended_windows\":2}\n";
        const EnergyReport r = kdosParseEnergyReport(reply);
        QVERIFY(r.ok);
        QCOMPARE(r.samples, 756);
        QCOMPARE(r.domains, QStringLiteral("package-0 + psys"));
        QCOMPARE(r.idleFloorWatts, 3.25);
        QVERIFY(r.floorSettled);
        QCOMPARE(r.suspendedWindows, 2);
        QCOMPARE(r.apps.size(), 4);
        QCOMPARE(r.apps[0].id, QStringLiteral("firefox@www"));
        QCOMPARE(r.apps[0].box, QStringLiteral("www"));
        QCOMPARE(r.apps[0].gpuShare, 0.221);
        QCOMPARE(r.apps[1].id, QStringLiteral("kwin_wayland"));
        QVERIFY(r.apps[1].box.isEmpty());
        QCOMPARE(r.apps[2].id, QStringLiteral("a_b_c"));
        QCOMPARE(r.apps[2].gpuShare, -1.0);
        QCOMPARE(r.apps[3].id, QStringLiteral("firefox@www~2"));
    }

    void energyRefusals()
    {
        const EnergyReport denied = kdosParseEnergyReport("err not permitted\n");
        QVERIFY(!denied.ok);
        QCOMPARE(denied.error, QStringLiteral("not permitted"));
        QVERIFY(!kdosParseEnergyReport("").ok);
        QVERIFY(!kdosParseEnergyReport("[1,2]").ok);
        QVERIFY(!kdosParseEnergyReport("{\"apps\":").ok);
    }

    void slotsOnTrial()
    {
        const QByteArray j =
            "{\"configured\": true, \"active\": \"a\", \"trying\": \"b\", \"attempts\": 2, \"bootnext\": true, "
            "\"slot_a\": \"/dev/vda2\", \"slot_b\": \"/dev/vda3\", \"crypt_a\": \"\", \"crypt_b\": \"\", "
            "\"running\": \"b\", \"next\": \"b\", \"failed\": null, \"entry\": \"kdos-b\", \"slots\": {"
            "\"a\": {\"state\": \"confirmed\", \"title\": \"KDOS 0.2.14\", \"version\": \"0.2.14\", \"build\": null, \"date\": null, "
            "\"tree\": null, \"tree_time\": 0, \"kernel\": null}, "
            "\"b\": {\"state\": \"trial\", \"title\": \"KDOS 0.2.15\", \"version\": \"0.2.15\", \"build\": null, \"date\": null, "
            "\"tree\": null, \"tree_time\": 0, \"kernel\": null}}}\n";
        const SlotState s = kdosParseSlotState(j);
        QVERIFY(s.configured);
        QCOMPARE(s.running, QStringLiteral("B"));
        QCOMPARE(s.runningVersion, QStringLiteral("0.2.15"));
        QCOMPARE(s.runningState, QStringLiteral("trial"));
        QCOMPARE(s.other, QStringLiteral("A"));
        QCOMPARE(s.otherVersion, QStringLiteral("0.2.14"));
        QCOMPARE(s.otherState, QStringLiteral("confirmed"));
        QCOMPARE(s.next, QStringLiteral("B"));
        QVERIFY(s.trial);
        QCOMPARE(s.attempts, 2);
    }

    void slotsSettledAndUnknown()
    {
        const SlotState s = kdosParseSlotState(
            "{\"configured\": true, \"active\": \"a\", \"trying\": null, \"attempts\": 0, \"running\": null, \"next\": \"a\", "
            "\"slots\": {\"a\": {\"state\": \"running\", \"version\": \"0.2.14\"}, \"b\": {\"state\": \"empty\", \"version\": null}}}");
        QVERIFY(s.configured);
        QCOMPARE(s.running, QStringLiteral("A"));
        QVERIFY(!s.trial);
        QVERIFY(s.otherVersion.isEmpty());
        QCOMPARE(s.otherState, QStringLiteral("empty"));

        QVERIFY(!kdosParseSlotState("{\"configured\": false}").configured);
        QVERIFY(!kdosParseSlotState("not json").configured);
        QVERIFY(!kdosParseSlotState("").configured);
    }
};

QTEST_GUILESS_MAIN(KdosStatsTest)

#include "kdosstatstest.moc"
