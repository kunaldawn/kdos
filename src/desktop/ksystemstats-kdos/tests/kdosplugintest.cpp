/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosplugintest — the sensor tree the plugin publishes
 *
 * The plugin is built into the test, pointed at an empty cgroup root, no
 * bootstate and no energy daemon, so what it publishes is its shape alone,
 * and the one-root or live-medium reading of the slots.
 */

#include "plugin.h"

#include <formatter/Formatter.h>

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Kdos;

class KdosPluginTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    KdosPlugin *m_plugin = nullptr;
    KSysGuard::SensorContainer *m_kdos = nullptr;

    void writeFile(const QString &name, const QByteArray &body)
    {
        QFile f(m_dir.filePath(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(body);
    }

    static QString value(const KSysGuard::SensorObject *o, const char *sensor)
    {
        const KSysGuard::SensorProperty *p = o->sensor(QString::fromLatin1(sensor));
        return p ? p->value().toString() : QStringLiteral("<no sensor>");
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        qputenv("KDOS_CGROUP_ROOT", m_dir.filePath(QStringLiteral("cgroup")).toUtf8());
        qputenv("KDOS_ENERGYD_SOCKET", m_dir.filePath(QStringLiteral("no-energyd.sock")).toUtf8());
        qputenv("KDOS_BOOTSTATE_PUB", m_dir.filePath(QStringLiteral("no-bootstate.json")).toUtf8());
        writeFile(QStringLiteral("mounts"), "/dev/vda2 / ext4 rw,relatime 0 0\nproc /proc proc rw 0 0\n");
        writeFile(QStringLiteral("mounts-live"), "overlay / overlay rw,lowerdir=/run/kdos/lower 0 0\n");
        writeFile(QStringLiteral("os-release"), "NAME=\"KDOS\"\nVERSION_ID=\"0.2\"\n");
        qputenv("KDOS_MOUNTS", m_dir.filePath(QStringLiteral("mounts")).toUtf8());
        qputenv("KDOS_OS_RELEASE", m_dir.filePath(QStringLiteral("os-release")).toUtf8());
        m_plugin = new KdosPlugin(this, {});
        /* An object joins its container from the event loop. */
        QCoreApplication::processEvents();
        const auto containers = m_plugin->containers();
        QCOMPARE(containers.size(), 1);
        m_kdos = containers.first();
        QCOMPARE(m_kdos->id(), QStringLiteral("kdos"));
    }

    /* The `kdos/box` and `kdos/energy` nodes are titled by objects of their
     * own; without them the sensor picker shows the raw paths. */
    void groupNodesHaveTitles_data()
    {
        QTest::addColumn<QString>("id");
        QTest::newRow("box") << QStringLiteral("box");
        QTest::newRow("energy") << QStringLiteral("energy");
        QTest::newRow("box/all") << QStringLiteral("box/all");
        QTest::newRow("energy/all") << QStringLiteral("energy/all");
    }

    void groupNodesHaveTitles()
    {
        QFETCH(QString, id);
        const KSysGuard::SensorObject *o = m_kdos->object(id);
        QVERIFY(o);
        QVERIFY(!o->name().isEmpty());
        QVERIFY(o->name() != id);
    }

    /* A string sensor whose unit the formatter takes for a number reads "0". */
    void textSensorsFormatAsText_data()
    {
        QTest::addColumn<QString>("object");
        QTest::addColumn<QString>("sensor");
        for (const char *s : {"status", "running", "runningVersion", "runningState", "other", "otherVersion", "otherState", "next"}) {
            QTest::newRow(s) << QStringLiteral("slot") << QString::fromLatin1(s);
        }
        QTest::newRow("phosphorState") << QStringLiteral("look") << QStringLiteral("phosphorState");
        QTest::newRow("energy status") << QStringLiteral("energy/all") << QStringLiteral("status");
        QTest::newRow("energy domains") << QStringLiteral("energy/all") << QStringLiteral("domains");
    }

    void textSensorsFormatAsText()
    {
        QFETCH(QString, object);
        QFETCH(QString, sensor);
        const KSysGuard::SensorObject *o = m_kdos->object(object);
        QVERIFY(o);
        const KSysGuard::SensorProperty *p = o->sensor(sensor);
        QVERIFY(p);
        QCOMPARE(p->info().unit, KSysGuard::UnitInvalid);
        const QString text = QStringLiteral("This machine boots one system");
        QCOMPARE(KSysGuard::Formatter::formatValue(text, p->info().unit, KSysGuard::MetricPrefixAutoAdjust, KSysGuard::FormatOptionShowNull), text);
    }

    void slotStatusWithoutSlots()
    {
        const KSysGuard::SensorObject *slot = m_kdos->object(QStringLiteral("slot"));
        QVERIFY(slot);
        const KSysGuard::SensorProperty *p = slot->sensor(QStringLiteral("status"));
        QVERIFY(p);
        const QString shown = KSysGuard::Formatter::formatValue(p->value(), p->info().unit, KSysGuard::MetricPrefixAutoAdjust, KSysGuard::FormatOptionShowNull);
        QVERIFY(!shown.isEmpty());
        QVERIFY(shown != QLatin1String("0"));
        QCOMPARE(shown, p->value().toString());
        QCOMPARE(shown, QStringLiteral("This machine boots one system; it has no A/B slots"));
        QCOMPARE(value(slot, "running"), QStringLiteral("One root"));
        QCOMPARE(value(slot, "runningVersion"), QStringLiteral("0.2"));
        QVERIFY(value(slot, "otherVersion").isEmpty());
    }

    /* The live medium has no boot state either, and says so as itself. */
    void slotStatusOnTheLiveMedium()
    {
        qputenv("KDOS_MOUNTS", m_dir.filePath(QStringLiteral("mounts-live")).toUtf8());
        auto *live = new KdosPlugin(this, {});
        QCoreApplication::processEvents();
        const KSysGuard::SensorObject *slot = live->containers().first()->object(QStringLiteral("slot"));
        QVERIFY(slot);
        QCOMPARE(value(slot, "status"), QStringLiteral("This is the live medium; it has no A/B slots"));
        QCOMPARE(value(slot, "running"), QStringLiteral("Live medium"));
        QCOMPARE(value(slot, "runningVersion"), QStringLiteral("0.2"));
        delete live;
        qputenv("KDOS_MOUNTS", m_dir.filePath(QStringLiteral("mounts")).toUtf8());
    }

    /* The System card reads Phosphor mode in a word; the 0/1 flag stays for
     * the face's scanline test. */
    void phosphorSaidInAWord()
    {
        const KSysGuard::SensorObject *look = m_kdos->object(QStringLiteral("look"));
        QVERIFY(look);
        QCOMPARE(value(look, "phosphorState"), QStringLiteral("Off"));
        QCOMPARE(look->sensor(QStringLiteral("phosphor"))->value().toInt(), 0);
    }

    /* Counts keep a numeric unit, which the formatter shows as a number. */
    void countsStayNumbers()
    {
        const KSysGuard::SensorObject *all = m_kdos->object(QStringLiteral("box/all"));
        const KSysGuard::SensorObject *look = m_kdos->object(QStringLiteral("look"));
        QVERIFY(all && all->sensor(QStringLiteral("boxes")));
        QVERIFY(look && look->sensor(QStringLiteral("phosphor")));
        QCOMPARE(all->sensor(QStringLiteral("boxes"))->info().unit, KSysGuard::UnitNone);
        QCOMPARE(look->sensor(QStringLiteral("phosphor"))->info().unit, KSysGuard::UnitNone);
    }
};

QTEST_GUILESS_MAIN(KdosPluginTest)

#include "kdosplugintest.moc"
