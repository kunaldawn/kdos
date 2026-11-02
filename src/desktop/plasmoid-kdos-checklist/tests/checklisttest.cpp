/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   checklisttest — the probe's answer, and the model over a fake probe
 */

#include "checklist.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Kdos;

class ChecklistTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    void setState(const QByteArray &lines)
    {
        QFile f(m_dir.filePath(QStringLiteral("state")));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(lines);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("PATH", QByteArray(QByteArray(KDOS_FAKE_KDOS_DIR) + ":/usr/bin:/bin"));
        qputenv("KDOS_FAKE_STATE", QFile::encodeName(m_dir.filePath(QStringLiteral("state"))));
        qputenv("KDOS_FAKE_LOG", QFile::encodeName(m_dir.filePath(QStringLiteral("log"))));
    }

    void parsesInTheListsOrder()
    {
        QList<ChecklistEntry> e;
        QVERIFY(kdosParseChecklist(
            "{\"version\":1,\"items\":[{\"id\":\"encryption\",\"done\":true},{\"id\":\"backup\",\"dismissed\":true},"
            "{\"id\":\"teleport\",\"done\":false},{\"id\":\"backup\",\"done\":true},{\"id\":\"box\"}]}",
            &e));
        QCOMPARE(e.size(), 3);
        QCOMPARE(e[0].id, QStringLiteral("backup"));
        QVERIFY(e[0].dismissed);
        QVERIFY(!e[0].done);
        QCOMPARE(e[1].id, QStringLiteral("box"));
        QCOMPARE(e[2].id, QStringLiteral("encryption"));
        QVERIFY(e[2].done);
    }

    void refusesWhatIsNotAList()
    {
        QList<ChecklistEntry> e;
        QVERIFY(!kdosParseChecklist("", &e));
        QVERIFY(!kdosParseChecklist("{\"items\":[]}", &e));
        QVERIFY(!kdosParseChecklist("{\"version\":9,\"items\":[]}", &e));
        QVERIFY(kdosParseChecklist("{\"version\":1}", &e));
        QVERIFY(e.isEmpty());
    }

    void everyIdHasWordsAndACommand()
    {
        for (const QString &id : kdosChecklistIds()) {
            const ChecklistText t = kdosChecklistText(id);
            QVERIFY2(!t.title.isEmpty(), qPrintable(id));
            QVERIFY2(!t.explanation.isEmpty(), qPrintable(id));
            QVERIFY2(!t.actionText.isEmpty(), qPrintable(id));
            QVERIFY2(!t.command.isEmpty(), qPrintable(id));
            QVERIFY2(!t.command.first().contains(QLatin1Char(' ')), qPrintable(id));
        }
        QVERIFY(kdosChecklistText(QStringLiteral("teleport")).title.isEmpty());
        QVERIFY(kdosChecklistText(QStringLiteral("phone")).optional);
    }

    void modelTicksItself()
    {
        setState("done firmware\n");
        ChecklistModel m;
        m.setInterval(1);
        QTRY_VERIFY_WITH_TIMEOUT(m.ready(), 5000);
        QVERIFY(m.problem().isEmpty());
        QCOMPARE(m.total(), 5);
        QCOMPARE(m.remaining(), 4);
        QVERIFY(!m.finished());
        QCOMPARE(m.data(m.index(0), ChecklistModel::IdRole).toString(), QStringLiteral("backup"));
        QCOMPARE(m.data(m.index(3), ChecklistModel::DoneRole).toBool(), true);

        /* The box is created elsewhere; the next probe ticks it. */
        QSignalSpy rows(&m, &QAbstractItemModel::dataChanged);
        setState("done firmware\ndone box\n");
        QTRY_COMPARE_WITH_TIMEOUT(m.remaining(), 3, 5000);
        QVERIFY(rows.count() >= 1);
        QCOMPARE(m.data(m.index(2), ChecklistModel::DoneRole).toBool(), true);
    }

    void dismissingEverythingFinishes()
    {
        setState("done firmware\ndone box\n");
        ChecklistModel m;
        m.setInterval(0);
        QTRY_VERIFY_WITH_TIMEOUT(m.ready(), 5000);
        for (const char *id : {"backup", "phone", "encryption"}) {
            m.dismiss(QString::fromLatin1(id));
        }
        m.dismiss(QStringLiteral("--not-an-item"));
        QTRY_VERIFY_WITH_TIMEOUT(m.finished(), 5000);
        QCOMPARE(m.remaining(), 0);

        QFile log(m_dir.filePath(QStringLiteral("log")));
        QVERIFY(log.open(QIODevice::ReadOnly));
        const QByteArray l = log.readAll();
        QVERIFY(l.contains("dismiss backup\n"));
        QVERIFY(!l.contains("--not-an-item"));

        m.restore();
        QTRY_COMPARE_WITH_TIMEOUT(m.remaining(), 3, 5000);
    }

    void failingProbeKeepsTheList()
    {
        setState("");
        ChecklistModel m;
        m.setInterval(0);
        QTRY_VERIFY_WITH_TIMEOUT(m.ready() && m.total() == 5, 5000);
        qputenv("KDOS_FAKE_FAIL", "1");
        m.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(m.problem(), QStringLiteral("kdos: checklist: lsblk is missing"), 5000);
        QCOMPARE(m.total(), 5);
        qunsetenv("KDOS_FAKE_FAIL");
    }

    void unknownItemOpensNothing()
    {
        ChecklistModel m;
        QVERIFY(!m.open(QStringLiteral("teleport")));
    }
};

QTEST_GUILESS_MAIN(ChecklistTest)

#include "checklisttest.moc"
