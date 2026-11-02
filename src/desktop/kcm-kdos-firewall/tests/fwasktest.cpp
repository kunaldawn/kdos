/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   fwasktest — kdos-fwask end to end, against fakes on one private bus
 *
 * The real binary runs as a child with the private session bus as its system
 * bus too, a fake /proc holding the programs it looks for, and a short
 * interval. Each case asserts what reached the fake notification server and
 * the fake daemon, and what was written to the answers file.
 */

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

#include "fakeservices.h"

class FwaskTest : public QObject
{
    Q_OBJECT

private:
    FakeBus *fakes = nullptr;
    QTemporaryDir *dir = nullptr;
    QProcess *ask = nullptr;

    void runProgram(const QString &comm, int pid)
    {
        const QString d = dir->filePath(QStringLiteral("proc/%1").arg(pid));
        QVERIFY(QDir().mkpath(d));
        QFile f(d + QStringLiteral("/comm"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(comm.toUtf8() + '\n');
    }

    void start()
    {
        ask = new QProcess;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), env.value(QStringLiteral("DBUS_SESSION_BUS_ADDRESS")));
        env.insert(QStringLiteral("XDG_STATE_HOME"), dir->filePath(QStringLiteral("state")));
        env.insert(QStringLiteral("KDOS_FWASK_PROC"), dir->filePath(QStringLiteral("proc")));
        env.insert(QStringLiteral("KDOS_FWASK_INTERVAL_MS"), QStringLiteral("150"));
        ask->setProcessEnvironment(env);
        ask->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        ask->start(QStringLiteral(KDOS_FWASK_BINARY), {});
        QVERIFY(ask->waitForStarted());
    }

    void stop()
    {
        if (!ask) {
            return;
        }
        ask->terminate();
        /* With the event loop running: the fakes live in this process, and a
         * call the child is in the middle of when the signal lands is only
         * answered, and the child only gets back to its loop to exit, while
         * they can reply. A blocking wait here starves them. */
        QTRY_VERIFY_WITH_TIMEOUT(ask->state() == QProcess::NotRunning, 8000);
        QCOMPARE(ask->exitStatus(), QProcess::NormalExit);
        QCOMPARE(ask->exitCode(), 0);
        delete ask;
        ask = nullptr;
    }

    QString memory()
    {
        QFile f(dir->filePath(QStringLiteral("state/kdos/fwask")));
        if (!f.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(f.readAll());
    }

    /* Let a few intervals pass with the event loop running, so the fakes
     * answer while the child looks. */
    void settle()
    {
        QTest::qWait(700);
    }

private Q_SLOTS:
    void init()
    {
        dir = new QTemporaryDir;
        QVERIFY(dir->isValid());
        QVERIFY(QDir().mkpath(dir->filePath(QStringLiteral("proc"))));
        fakes = new FakeBus;
        QVERIFY(fakes->servePower1());
        QVERIFY(fakes->servePolkit());
        QVERIFY(fakes->serveNetworkManager());
        QVERIFY(fakes->serveNotifications());
    }

    void cleanup()
    {
        stop();
        delete fakes;
        fakes = nullptr;
        delete dir;
        dir = nullptr;
    }

    void asksOnceWhenAKnownProgramIsTurnedAway()
    {
        runProgram(QStringLiteral("kdeconnectd"), 4242);
        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 3);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        const Notification n = fakes->notifications.shown.first();
        QCOMPARE(n.summary, QStringLiteral("KDE Connect can't be reached by your phone"));
        QVERIFY(n.body.contains(QStringLiteral("“Home &lt;5G&gt;”")));
        QVERIFY(n.actions.contains(QStringLiteral("allow")));
        QVERIFY(n.actions.contains(QStringLiteral("deny")));
        QCOMPARE(n.timeout, 0);
        QCOMPARE(n.hints.value(QStringLiteral("desktop-entry")).toString(), QStringLiteral("kcm_kdos_firewall"));

        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 30);
        settle();
        QCOMPARE(fakes->notifications.shown.size(), 1);
    }

    void allowOpensItByNameAndRemembers()
    {
        runProgram(QStringLiteral("kdeconnectd"), 4242);
        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 3);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        fakes->notifications.invoke(fakes->connection, fakes->notifications.shown.first().id, QStringLiteral("allow"));
        QTRY_COMPARE(fakes->power.setCalls.size(), 1);
        QCOMPARE(fakes->power.setCalls.first().name, QStringLiteral("kdeconnect"));
        QVERIFY(fakes->power.setCalls.first().open);
        QVERIFY(fakes->power.setCalls.first().interactive);
        QTRY_COMPARE(fakes->notifications.shown.size(), 2);
        QCOMPARE(fakes->notifications.shown.at(1).summary, QStringLiteral("KDE Connect can now be reached"));
        QTRY_VERIFY(memory().contains(fakes->nm.uuid + QStringLiteral(" kdeconnect allowed\n")));
    }

    void neverAsksTwiceOnOneNetwork()
    {
        runProgram(QStringLiteral("krdpserver"), 5151);
        fakes->power.knocks.insert(QStringLiteral("rdp"), 1);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        fakes->notifications.invoke(fakes->connection, fakes->notifications.shown.first().id, QStringLiteral("deny"));
        QTRY_VERIFY(memory().contains(fakes->nm.uuid + QStringLiteral(" rdp declined\n")));
        fakes->power.knocks.insert(QStringLiteral("rdp"), 50);
        settle();
        QCOMPARE(fakes->notifications.shown.size(), 1);
        QCOMPARE(fakes->power.setCalls.size(), 0);

        /* A restart reads the answer back. */
        stop();
        fakes->power.knocks.insert(QStringLiteral("rdp"), 80);
        start();
        settle();
        QCOMPARE(fakes->notifications.shown.size(), 1);

        /* Another network is another question. */
        fakes->nm.uuid = QStringLiteral("0a0b0c0d-1111-2222-3333-444455556666");
        fakes->nm.id = QStringLiteral("Cafe");
        fakes->power.knocks.insert(QStringLiteral("rdp"), 81);
        QTRY_COMPARE(fakes->notifications.shown.size(), 2);
        QVERIFY(fakes->notifications.shown.at(1).body.contains(QStringLiteral("“Cafe”")));
    }

    void dismissingIsAnAnswer()
    {
        runProgram(QStringLiteral("krfb"), 6161);
        fakes->power.knocks.insert(QStringLiteral("vnc"), 2);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        fakes->notifications.close(fakes->connection, fakes->notifications.shown.first().id, 2);
        QTRY_VERIFY(memory().contains(QStringLiteral(" vnc dismissed\n")));
    }

    void quietWithoutTheProgramOrForAnUnknownService()
    {
        fakes->power.knocks.insert(QStringLiteral("vnc"), 7);
        fakes->power.knocks.insert(QStringLiteral("http"), 900);
        runProgram(QStringLiteral("httpd"), 7171);
        runProgram(QStringLiteral("kdeconnectd-x"), 7272);
        start();
        settle();
        QCOMPARE(fakes->notifications.shown.size(), 0);
        QVERIFY(memory().isEmpty());
    }

    void tellsSomeoneWhoCannotOpenIt()
    {
        fakes->polkit.challenge = false;
        runProgram(QStringLiteral("kdeconnectd"), 4242);
        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 3);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        const Notification n = fakes->notifications.shown.first();
        QVERIFY(!n.actions.contains(QStringLiteral("allow")));
        QVERIFY(n.body.contains(QStringLiteral("only an administrator")));
        QTRY_VERIFY(memory().contains(QStringLiteral(" kdeconnect informed\n")));
    }

    void withdrawsAQuestionWhenTheNetworkChanges()
    {
        runProgram(QStringLiteral("kdeconnectd"), 4242);
        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 3);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        fakes->nm.uuid = QStringLiteral("0a0b0c0d-1111-2222-3333-444455556666");
        QTRY_COMPARE(fakes->notifications.closed.size(), 1);
        QCOMPARE(fakes->notifications.closed.first(), fakes->notifications.shown.first().id);
        QVERIFY(memory().isEmpty());
    }

    void reportsARefusal()
    {
        fakes->power.failName = QStringLiteral("kdeconnect");
        fakes->power.failMessage = QStringLiteral("the ruleset would not load <here>");
        runProgram(QStringLiteral("kdeconnectd"), 4242);
        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 3);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        fakes->notifications.invoke(fakes->connection, fakes->notifications.shown.first().id, QStringLiteral("allow"));
        QTRY_COMPARE(fakes->notifications.shown.size(), 2);
        QCOMPARE(fakes->notifications.shown.at(1).summary, QStringLiteral("The firewall was not changed"));
        QCOMPARE(fakes->notifications.shown.at(1).body, QStringLiteral("the ruleset would not load &lt;here&gt;"));
        QTRY_VERIFY(memory().contains(QStringLiteral(" kdeconnect failed\n")));
    }

    void anAnswerFromAnotherConnectionIsIgnored()
    {
        runProgram(QStringLiteral("kdeconnectd"), 4242);
        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 3);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        QDBusMessage forged = QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/Notifications"),
                                                         QStringLiteral("org.freedesktop.Notifications"),
                                                         QStringLiteral("ActionInvoked"));
        forged << fakes->notifications.shown.first().id << QStringLiteral("allow");
        QDBusConnection::sessionBus().send(forged);
        settle();
        QCOMPARE(fakes->power.setCalls.size(), 0);
    }

    void openedElsewhereWithdrawsTheQuestion()
    {
        runProgram(QStringLiteral("kdeconnectd"), 4242);
        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 3);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        const quint32 id = fakes->notifications.shown.first().id;

        /* Opened with no word yet; then the signal, from a connection that
         * does not own the daemon's name, withdraws nothing. */
        for (Kdos::FirewallService &s : fakes->power.services) {
            if (s.name == QLatin1String("kdeconnect")) {
                s.open = true;
            }
        }
        QDBusConnection::sessionBus().send(
            QDBusMessage::createSignal(QStringLiteral("/io/kdos/Power1"), QStringLiteral("io.kdos.Power1"), QStringLiteral("FirewallServicesChanged")));
        settle();
        QVERIFY(fakes->notifications.closed.isEmpty());

        fakes->power.emitChanged(fakes->connection);
        QTRY_COMPARE(fakes->notifications.closed.size(), 1);
        QCOMPARE(fakes->notifications.closed.first(), id);
        QTRY_VERIFY(memory().contains(fakes->nm.uuid + QStringLiteral(" kdeconnect elsewhere\n")));
        QCOMPARE(fakes->power.setCalls.size(), 0);
        QCOMPARE(fakes->notifications.shown.size(), 1);
    }

    void aChangeToAnotherServiceIsNotAnAnswer()
    {
        runProgram(QStringLiteral("kdeconnectd"), 4242);
        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 3);
        start();
        QTRY_COMPARE(fakes->notifications.shown.size(), 1);
        fakes->power.setElsewhere(fakes->connection, QStringLiteral("ssh"), true);
        fakes->power.setElsewhere(fakes->connection, QStringLiteral("kdeconnect"), false);
        settle();
        QVERIFY(fakes->notifications.closed.isEmpty());
        QVERIFY(memory().isEmpty());
    }
};

QTEST_GUILESS_MAIN(FwaskTest)

#include "fwasktest.moc"
