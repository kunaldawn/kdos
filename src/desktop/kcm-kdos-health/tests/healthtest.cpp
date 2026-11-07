/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   healthtest — the Health page's logic against a fake `kdos doctor`
 *
 * The reports are written in the shape kdos.c's doctor_record() emits:
 * {"checks": [{"section", "level", "message"}...], "warnings": N}.
 */

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "doctormodel.h"
#include "healthcontroller.h"

using Kdos::DoctorModel;
using Kdos::HealthController;

namespace
{
const QByteArray Report = R"({
  "checks": [
    {"section": "Kernel", "level": "ok", "message": "Landlock ABI 6"},
    {"section": "Hardware", "level": "skip", "message": "no Wi-Fi adapter — regulatory domain not checked"},
    {"section": "Session", "level": "ok", "message": "kwin_wayland running"},
    {"section": "Session", "level": "warn", "message": "plasmashell not running — no panel or launcher"},
    {"section": "Containers", "level": "warn", "message": "podman info fails — no store"}
  ],
  "warnings": 2
}
)";
}

class HealthTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir *dir = nullptr;

    void fixture(const QByteArray &body, int exitCode, const QByteArray &err = QByteArray())
    {
        const QString path = dir->filePath(QStringLiteral("report.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(body);
        f.close();
        qputenv("FAKE_DOCTOR_FILE", path.toLocal8Bit());
        qputenv("FAKE_DOCTOR_EXIT", QByteArray::number(exitCode));
        if (err.isEmpty()) {
            qunsetenv("FAKE_DOCTOR_ERR");
        } else {
            qputenv("FAKE_DOCTOR_ERR", err);
        }
    }

    void runAndWait(HealthController &c)
    {
        c.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(c.ready(), 10000);
    }

    QStringList runs()
    {
        QFile f(dir->filePath(QStringLiteral("argv.log")));
        if (!f.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

private Q_SLOTS:
    void init()
    {
        dir = new QTemporaryDir;
        QVERIFY(dir->isValid());
        qputenv("FAKE_DOCTOR_LOG", dir->filePath(QStringLiteral("argv.log")).toLocal8Bit());
        qunsetenv("FAKE_DOCTOR_SLEEP");
    }

    void cleanup()
    {
        delete dir;
        dir = nullptr;
    }

    void aReportWithWarningsIsAReport()
    {
        fixture(Report, 1);
        HealthController c(QStringLiteral(KDOS_FAKE_DOCTOR));
        runAndWait(c);
        QVERIFY2(c.error().isEmpty(), qPrintable(c.error()));
        QCOMPARE(c.total(), 5);
        QCOMPARE(c.warnings(), 2);
        QCOMPARE(c.passed(), 2);
        QCOMPARE(c.skipped(), 1);
        QAbstractItemModel *m = c.checks();
        QCOMPARE(m->rowCount(), 5);
        QCOMPARE(m->data(m->index(0, 0), DoctorModel::SectionRole).toString(), QStringLiteral("Kernel"));
        QCOMPARE(m->data(m->index(1, 0), DoctorModel::LevelRole).toString(), QStringLiteral("skip"));
        QCOMPARE(m->data(m->index(3, 0), DoctorModel::LevelRole).toString(), QStringLiteral("warn"));
        QCOMPARE(m->data(m->index(3, 0), DoctorModel::MessageRole).toString(), QStringLiteral("plasmashell not running — no panel or launcher"));
        QCOMPARE(runs(), QStringList{QStringLiteral("doctor --json")});
    }

    void aCleanReport()
    {
        fixture(R"({"checks": [{"section": "Kernel", "level": "ok", "message": "Landlock ABI 6"}], "warnings": 0})", 0);
        HealthController c(QStringLiteral(KDOS_FAKE_DOCTOR));
        runAndWait(c);
        QVERIFY(c.error().isEmpty());
        QCOMPARE(c.total(), 1);
        QCOMPARE(c.warnings(), 0);
    }

    void anUnknownLevelIsShownAsAWarning()
    {
        fixture(R"({"checks": [{"section": "Boxes", "level": "fatal", "message": "the store is gone"}, {"section": "Boxes", "level": "ok", "message": ""}]})", 1);
        HealthController c(QStringLiteral(KDOS_FAKE_DOCTOR));
        runAndWait(c);
        QCOMPARE(c.total(), 1);
        QCOMPARE(c.warnings(), 1);
    }

    void aFailedDoctorIsAnErrorWithItsReason()
    {
        fixture(QByteArray(), 2, "usage: kdos doctor [--json] [--cve]");
        HealthController c(QStringLiteral(KDOS_FAKE_DOCTOR));
        runAndWait(c);
        QVERIFY(c.error().contains(QStringLiteral("exit status 2")));
        QCOMPARE(c.explanation(), QStringLiteral("usage: kdos doctor [--json] [--cve]"));
        QCOMPARE(c.total(), 0);
        QVERIFY(c.text().isEmpty());
    }

    void outputThatIsNotAReportIsAnError()
    {
        fixture("KDOS doctor\n\nKernel\n  [ ok ] Landlock ABI 6\n", 0);
        HealthController c(QStringLiteral(KDOS_FAKE_DOCTOR));
        runAndWait(c);
        QVERIFY(c.error().contains(QStringLiteral("no report")));
        QCOMPARE(c.total(), 0);
    }

    void aMissingProgramIsSaid()
    {
        HealthController c(dir->filePath(QStringLiteral("no-such-kdos")));
        runAndWait(c);
        QVERIFY(c.error().contains(QStringLiteral("could not be run")));
        QVERIFY(!c.explanation().isEmpty());
    }

    void theFilterMatchesMessagesAndSections()
    {
        fixture(Report, 1);
        HealthController c(QStringLiteral(KDOS_FAKE_DOCTOR));
        runAndWait(c);
        c.setFilter(QStringLiteral("PODMAN"));
        QCOMPARE(c.checks()->rowCount(), 1);
        c.setFilter(QStringLiteral("session"));
        QCOMPARE(c.checks()->rowCount(), 2);
        c.setFilter(QString());
        QCOMPARE(c.checks()->rowCount(), 5);
        QCOMPARE(c.total(), 5);
    }

    void theTextIsDoctorsOwn()
    {
        fixture(Report, 1);
        HealthController c(QStringLiteral(KDOS_FAKE_DOCTOR));
        runAndWait(c);
        const QString want = QStringLiteral(
            "KDOS doctor\n"
            "\nKernel\n"
            "  [ ok ] Landlock ABI 6\n"
            "\nHardware\n"
            "  [skip] no Wi-Fi adapter — regulatory domain not checked\n"
            "\nSession\n"
            "  [ ok ] kwin_wayland running\n"
            "  [warn] plasmashell not running — no panel or launcher\n"
            "\nContainers\n"
            "  [warn] podman info fails — no store\n");
        QCOMPARE(c.text(), want);
        c.copyToClipboard();
        QCOMPARE(QGuiApplication::clipboard()->text(), want);
    }

    void aRefreshWhileRunningRunsOnceMoreAfter()
    {
        fixture(Report, 1);
        qputenv("FAKE_DOCTOR_SLEEP", "0.3");
        HealthController c(QStringLiteral(KDOS_FAKE_DOCTOR));
        QSignalSpy result(&c, &HealthController::resultChanged);
        c.refresh();
        c.refresh();
        c.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(runs().size(), 2, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(c.ready(), 10000);
        QTest::qWait(500);
        QCOMPARE(runs().size(), 2);
    }
};

QTEST_MAIN(HealthTest)

#include "healthtest.moc"
