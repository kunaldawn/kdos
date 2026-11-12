/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — the decisions, against fixtures
 * ---------------------------------
 */

#include "logic.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace Kdos;

class LogicTest : public QObject
{
    Q_OBJECT

private:
    static void write(const QString &path, const QByteArray &data)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

private Q_SLOTS:
    void accentFromScheme_data()
    {
        QTest::addColumn<QString>("scheme");
        QTest::addColumn<QString>("accent");

        QTest::newRow("bone") << QStringLiteral("KDOSBone") << QStringLiteral("bone");
        QTest::newRow("phosphor") << QStringLiteral("KDOSPhosphor") << QStringLiteral("phosphor");
        QTest::newRow("breeze") << QStringLiteral("BreezeDark") << QString();
        QTest::newRow("box title bar") << QStringLiteral("KDOSBox") << QString();
        QTest::newRow("bare prefix") << QStringLiteral("KDOS") << QString();
        QTest::newRow("lowercase") << QStringLiteral("KDOSamber") << QString();
        QTest::newRow("two words") << QStringLiteral("KDOSAmberEdited") << QString();
        QTest::newRow("spaced") << QStringLiteral("KDOS Amber") << QString();
        QTest::newRow("path") << QStringLiteral("KDOSAmber/../x") << QString();
        QTest::newRow("empty") << QString() << QString();
    }

    void accentFromScheme()
    {
        QFETCH(QString, scheme);
        QFETCH(QString, accent);
        QCOMPARE(Kdos::accentFromScheme(scheme), accent);
    }

    void shippedScheme()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString user = root.filePath(QStringLiteral("home/.local/share"));
        const QString sys = root.filePath(QStringLiteral("usr/share"));

        write(sys + QStringLiteral("/color-schemes/KDOSAmber.colors"), "[General]\n");
        write(user + QStringLiteral("/color-schemes/KDOSMine.colors"), "[General]\n");
        write(user + QStringLiteral("/color-schemes/KDOSAmber.colors"), "[General]\n");

        const QStringList dirs{user, sys};

        QVERIFY(isShippedScheme(QStringLiteral("KDOSAmber"), user, dirs));
        QVERIFY(!isShippedScheme(QStringLiteral("KDOSMine"), user, dirs));
        QVERIFY(!isShippedScheme(QStringLiteral("KDOSIce"), user, dirs));
        QVERIFY(!isShippedScheme(QStringLiteral("../KDOSAmber"), user, dirs));
        QVERIFY(!isShippedScheme(QString(), user, dirs));
        /* The user's directory spelt with a trailing slash is still the
         * user's directory. */
        QVERIFY(!isShippedScheme(QStringLiteral("KDOSMine"), user + QLatin1Char('/'), dirs));
    }

    void readAccent()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("accent"));

        QCOMPARE(Kdos::readAccent(path), QString());
        write(path, "amber\n");
        QCOMPARE(Kdos::readAccent(path), QStringLiteral("amber"));
        write(path, "  ice  \n\nextra\n");
        QCOMPARE(Kdos::readAccent(path), QStringLiteral("ice"));
        write(path, "\n\n");
        QCOMPARE(Kdos::readAccent(path), QString());
    }

    void ktarSets()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString d = root.path();

        write(d + QStringLiteral("/b.ktar"), "x");
        write(d + QStringLiteral("/a.ktar"), "x");
        write(d + QStringLiteral("/SETS.KTAR"), "x");
        write(d + QStringLiteral("/c.ktar"), "x");
        write(d + QStringLiteral("/d.ktar"), "x");
        write(d + QStringLiteral("/e.ktar.txt"), "x");
        write(d + QStringLiteral("/empty.ktar"), "");
        write(d + QStringLiteral("/.hidden.ktar"), "x");
        write(d + QStringLiteral("/notes.txt"), "x");
        write(d + QStringLiteral("/sub/deep.ktar"), "x");
        QVERIFY(QDir(d).mkdir(QStringLiteral("dir.ktar")));
        QVERIFY(QFile::link(d + QStringLiteral("/b.ktar"), d + QStringLiteral("/link.ktar")));

        const QStringList all = Kdos::ktarSets(d, 10, 4096);
        const QStringList want{d + QStringLiteral("/a.ktar"), d + QStringLiteral("/b.ktar"),
                               d + QStringLiteral("/c.ktar"), d + QStringLiteral("/d.ktar")};
        QCOMPARE(all, want);

        QCOMPARE(Kdos::ktarSets(d, 2, 4096), want.mid(0, 2));
        QVERIFY(Kdos::ktarSets(d, 10, 0).isEmpty());
        QVERIFY(Kdos::ktarSets(d + QStringLiteral("/missing"), 10, 4096).isEmpty());
    }

    void startAllowed()
    {
        QList<qint64> starts;

        for (int i = 0; i < 5; ++i) {
            QVERIFY(Kdos::startAllowed(starts, 1000 + i * 1000, 5, 60000));
        }
        QVERIFY(!Kdos::startAllowed(starts, 10000, 5, 60000));
        QCOMPARE(starts.size(), 5);

        /* The first start leaves the window at 61000, and makes room for one. */
        QVERIFY(!Kdos::startAllowed(starts, 60999, 5, 60000));
        QVERIFY(Kdos::startAllowed(starts, 61000, 5, 60000));
        QVERIFY(!Kdos::startAllowed(starts, 61001, 5, 60000));
        QVERIFY(Kdos::startAllowed(starts, 200000, 5, 60000));
        QCOMPARE(starts.size(), 1);
    }
};

QTEST_GUILESS_MAIN(LogicTest)

#include "logictest.moc"
