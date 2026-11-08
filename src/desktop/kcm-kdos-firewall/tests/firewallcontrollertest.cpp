/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   firewallcontrollertest — the Firewall page's logic against a fake daemon
 *
 * Run under dbus-run-session (testing/plasmatest.sh does): the fakes and the
 * controller are two connections to that private bus.
 */

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QTest>

#include "fakeservices.h"
#include "firewallcontroller.h"
#include "power1client.h"

using Kdos::FirewallController;
using Kdos::FirewallServiceModel;

class FirewallControllerTest : public QObject
{
    Q_OBJECT

private:
    FakeBus *fakes = nullptr;

    int rowOf(FirewallServiceModel *m, const QString &name)
    {
        for (int i = 0; i < m->rowCount(); ++i) {
            if (m->data(m->index(i), FirewallServiceModel::NameRole).toString() == name) {
                return i;
            }
        }
        return -1;
    }

    bool applied(FirewallServiceModel *m, const QString &name)
    {
        return m->data(m->index(rowOf(m, name)), FirewallServiceModel::AppliedRole).toBool();
    }

    void loadAndWait(FirewallController &c)
    {
        c.load();
        QTRY_VERIFY(!c.loading());
    }

private Q_SLOTS:
    void init()
    {
        QVERIFY(QDBusConnection::sessionBus().isConnected());
        fakes = new FakeBus;
        QVERIFY(fakes->connection.isConnected());
        QVERIFY(fakes->servePower1());
        QVERIFY(fakes->servePolkit());
    }

    void cleanup()
    {
        delete fakes;
        fakes = nullptr;
    }

    void listsTheDaemonsRowsInItsOrder()
    {
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        QVERIFY(c.available());
        FirewallServiceModel *m = c.services();
        QCOMPARE(m->rowCount(), 5);
        QCOMPARE(m->data(m->index(0), FirewallServiceModel::NameRole).toString(), QStringLiteral("ssh"));
        QCOMPARE(m->data(m->index(4), FirewallServiceModel::NameRole).toString(), QStringLiteral("rdp"));
        QCOMPARE(m->data(m->index(2), FirewallServiceModel::DescriptionRole).toString(), QStringLiteral("KDE Connect with a phone on the LAN"));
        QVERIFY(!c.isDirty());
        QVERIFY(c.representsDefaults());
    }

    void asksPolkitAboutTheActionForItsOwnConnection()
    {
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        QCOMPARE(fakes->polkit.actions.value(0), QStringLiteral("io.kdos.firewall.set"));
        QCOMPARE(fakes->polkit.subjects.value(0), QStringLiteral("system-bus-name:") + QDBusConnection::sessionBus().baseService());
        QVERIFY(c.canChange());
        QVERIFY(c.readOnlyReason().isEmpty());
    }

    void appliesOneCallPerChangedRowWithAPrompt()
    {
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        FirewallServiceModel *m = c.services();
        QSignalSpy dirty(&c, &FirewallController::dirtyChanged);
        c.setServiceOpen(rowOf(m, QStringLiteral("ssh")), true);
        c.setServiceOpen(rowOf(m, QStringLiteral("kdeconnect")), true);
        QVERIFY(c.isDirty());
        QVERIFY(!c.representsDefaults());
        QVERIFY(dirty.count() >= 2);
        QVERIFY(!applied(m, QStringLiteral("ssh")));

        QSignalSpy done(&c, &FirewallController::applied);
        c.apply();
        QVERIFY(done.wait());
        QCOMPARE(done.first().first().toBool(), true);
        QCOMPARE(fakes->power.setCalls.size(), 2);
        QCOMPARE(fakes->power.setCalls.at(0).name, QStringLiteral("ssh"));
        QVERIFY(fakes->power.setCalls.at(0).open);
        QVERIFY(fakes->power.setCalls.at(0).interactive);
        QCOMPARE(fakes->power.setCalls.at(1).name, QStringLiteral("kdeconnect"));
        QVERIFY(applied(m, QStringLiteral("ssh")));
        QVERIFY(applied(m, QStringLiteral("kdeconnect")));
        QVERIFY(!c.isDirty());
        QVERIFY(c.errorMessage().isEmpty());
    }

    void aReloadDuringAnApplyDoesNotEndIt()
    {
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        FirewallServiceModel *m = c.services();
        c.setServiceOpen(rowOf(m, QStringLiteral("ssh")), true);
        c.setServiceOpen(rowOf(m, QStringLiteral("kdeconnect")), true);

        QSignalSpy done(&c, &FirewallController::applied);
        c.apply();
        c.load();
        QVERIFY(done.wait());
        QCOMPARE(done.count(), 1);
        QCOMPARE(fakes->power.setCalls.size(), 2);
        QVERIFY(applied(m, QStringLiteral("kdeconnect")));
        QVERIFY(!c.isDirty());
    }

    void stopsAtTheFirstRefusalAndShowsTheTruth()
    {
        fakes->power.failName = QStringLiteral("http");
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        FirewallServiceModel *m = c.services();
        c.setServiceOpen(rowOf(m, QStringLiteral("ssh")), true);
        c.setServiceOpen(rowOf(m, QStringLiteral("http")), true);
        c.setServiceOpen(rowOf(m, QStringLiteral("rdp")), true);

        QSignalSpy done(&c, &FirewallController::applied);
        c.apply();
        QVERIFY(done.wait());
        QCOMPARE(done.first().first().toBool(), false);
        QCOMPARE(fakes->power.setCalls.size(), 2);
        QCOMPARE(fakes->power.setCalls.at(1).name, QStringLiteral("http"));
        QVERIFY(c.errorMessage().contains(QStringLiteral("the ruleset would not load; nothing changed")));
        QVERIFY(c.errorMessage().contains(QStringLiteral("http")));
        /* ssh went through, http was refused, rdp was never sent; the list
         * that came back says exactly that. */
        QVERIFY(applied(m, QStringLiteral("ssh")));
        QVERIFY(!applied(m, QStringLiteral("http")));
        QVERIFY(!applied(m, QStringLiteral("rdp")));
        QVERIFY(!c.isDirty());
        c.dismissError();
        QVERIFY(c.errorMessage().isEmpty());
    }

    void readOnlyWhenPolkitSaysNo()
    {
        fakes->polkit.challenge = false;
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        QVERIFY(c.available());
        QVERIFY(!c.canChange());
        QVERIFY(!c.readOnlyReason().isEmpty());
        c.setServiceOpen(0, true);
        QVERIFY(!c.isDirty());
        c.apply();
        QCOMPARE(fakes->power.setCalls.size(), 0);
    }

    void unavailableWithoutTheDaemon()
    {
        fakes->withdrawPower1();
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        QVERIFY(!c.available());
        QVERIFY(!c.canChange());
        QVERIFY(c.unavailableReason().contains(QStringLiteral("kdos-powerd")));
        QCOMPARE(c.services()->rowCount(), 0);
    }

    void showsKnocksOnClosedServicesOnly()
    {
        fakes->power.knocks.insert(QStringLiteral("kdeconnect"), 12);
        fakes->power.services[0].open = true;
        fakes->power.knocks.insert(QStringLiteral("ssh"), 99);
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        FirewallServiceModel *m = c.services();
        QTRY_COMPARE(m->data(m->index(rowOf(m, QStringLiteral("kdeconnect"))), FirewallServiceModel::KnocksRole).toDouble(), 12.0);
        QCOMPARE(m->data(m->index(rowOf(m, QStringLiteral("ssh"))), FirewallServiceModel::KnocksRole).toDouble(), 0.0);
        QCOMPARE(m->openCount(), 1);
    }

    void aDaemonWithoutCountersIsNotAnError()
    {
        fakes->power.knocksSupported = false;
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        QVERIFY(c.available());
        QVERIFY(c.errorMessage().isEmpty());
    }

    void followsAChangeMadeElsewhereUnlessHoldingChanges()
    {
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        FirewallServiceModel *m = c.services();

        fakes->power.setElsewhere(fakes->connection, QStringLiteral("http"), true);
        QTRY_VERIFY(applied(m, QStringLiteral("http")));

        c.setServiceOpen(rowOf(m, QStringLiteral("ssh")), true);
        fakes->power.setElsewhere(fakes->connection, QStringLiteral("rdp"), true);
        QTest::qWait(300);
        QVERIFY(c.isDirty());
        QVERIFY(!applied(m, QStringLiteral("rdp")));
    }

    void aSignalFromAnotherConnectionIsNotTheDaemons()
    {
        /* Any connection may emit a signal with the daemon's path and
         * interface; the page hears the one that owns io.kdos.Power1. */
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        FirewallServiceModel *m = c.services();
        fakes->power.services[1].open = true;
        {
            QDBusConnection forger = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("forger"));
            QVERIFY(forger.isConnected());
            forger.send(QDBusMessage::createSignal(QStringLiteral("/io/kdos/Power1"), QStringLiteral("io.kdos.Power1"), QStringLiteral("FirewallServicesChanged")));
            QTest::qWait(300);
        }
        QDBusConnection::disconnectFromBus(QStringLiteral("forger"));
        QVERIFY(!applied(m, QStringLiteral("http")));
        fakes->power.emitChanged(fakes->connection);
        QTRY_VERIFY(applied(m, QStringLiteral("http")));
    }

    void defaultsCloseEverything()
    {
        fakes->power.services[0].open = true;
        fakes->power.services[2].open = true;
        FirewallController c(QDBusConnection::sessionBus());
        loadAndWait(c);
        QVERIFY(!c.representsDefaults());
        c.defaults();
        QVERIFY(c.representsDefaults());
        QCOMPARE(c.services()->pendingChanges().size(), 2);
        c.revert();
        QVERIFY(!c.isDirty());
    }

    void describesErrorsAsSentences()
    {
        QVERIFY(Kdos::Power1Client::describe(QDBusError(QDBusError::AccessDenied, QString())).contains(QStringLiteral("authentication")));
        QVERIFY(Kdos::Power1Client::describe(QDBusError(QDBusError::ServiceUnknown, QString())).contains(QStringLiteral("service start powerd")));
        QCOMPARE(Kdos::Power1Client::describe(QDBusError(QDBusError::Other, QStringLiteral("disk full"))), QStringLiteral("disk full"));
    }
};

QTEST_GUILESS_MAIN(FirewallControllerTest)

#include "firewallcontrollertest.moc"
