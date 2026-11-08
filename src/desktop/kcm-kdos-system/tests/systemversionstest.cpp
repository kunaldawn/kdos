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
 * The System Versions module against a fixture io.kdos.Slots1.
 *
 * EVERY TEST RUNS A REAL SERVER PROCESS on the session bus (fakeslots1, built
 * from the interface XML), so the module's calls cross the bus exactly as
 * they do against kdos-sysupdated, and what a test asserts on is the boot
 * state file the server wrote, in kdos-bootctl's layout.
 *
 * A MISSING SESSION BUS FAILS, it does not skip: a suite that passes because
 * it ran nothing is the one result that must never look like a pass. Run it
 * under dbus-run-session, which is what testing/plasmatest.sh does.
 */

#include "slotinfo.h"
#include "slotsclient.h"
#include "systemversions.h"

#include <KPluginMetaData>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
bool copyTree(const QString &from, const QString &to)
{
    QDir src(from);
    if (!QDir().mkpath(to)) {
        return false;
    }
    const auto entries = src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &fi : entries) {
        const QString dst = to + u'/' + fi.fileName();
        if (fi.isDir()) {
            if (!copyTree(fi.filePath(), dst)) {
                return false;
            }
        } else if (!QFile::copy(fi.filePath(), dst)) {
            return false;
        }
    }
    return true;
}

bool nameOwned()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(u"io.kdos.Slots1"_s);
}
}

class SystemVersionsTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QProcess> m_server;
    std::unique_ptr<Kdos::SystemVersionsModule> m_kcm;

    QString path(const QString &name) const
    {
        return m_dir->path() + u'/' + name;
    }

    QString read(const QString &name) const
    {
        QFile f(path(name));
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    }

    void write(const QString &name, const QString &text) const
    {
        QFile f(path(name));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(text.toUtf8());
    }

    void startServer()
    {
        m_server = std::make_unique<QProcess>();
        m_server->setProcessChannelMode(QProcess::ForwardedChannels);
        m_server->start(QStringLiteral(FAKESLOTS1_PATH), {m_dir->path()});
        QVERIFY2(m_server->waitForStarted(), qPrintable(m_server->errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(nameOwned(), 10000);
    }

    void stopServer()
    {
        if (!m_server) {
            return;
        }
        m_server->terminate();
        if (!m_server->waitForFinished(5000)) {
            m_server->kill();
            m_server->waitForFinished(5000);
        }
        m_server.reset();
        QTRY_VERIFY_WITH_TIMEOUT(!nameOwned(), 10000);
    }

    /* A fresh copy of `fixture`, its server, and a loaded module. */
    void begin(const QString &fixture = u"ab"_s)
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        QVERIFY(copyTree(QStringLiteral(FIXTURES_DIR) + u'/' + fixture, m_dir->path()));
        startServer();
        m_kcm = std::make_unique<Kdos::SystemVersionsModule>(nullptr, KPluginMetaData());
        m_kcm->load();
        QTRY_VERIFY(m_kcm->isLoaded());
        QTRY_VERIFY(m_kcm->isAvailable());
    }

    Kdos::SlotInfo *slot(int i) const
    {
        return qobject_cast<Kdos::SlotInfo *>(m_kcm->slotList().value(i));
    }

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("KDOS_SLOTS1_BUS", "session");
        QVERIFY2(QDBusConnection::sessionBus().isConnected(), "this suite needs a session bus: run it under dbus-run-session");
        QVERIFY2(!nameOwned(), "something else already owns io.kdos.Slots1 on this session bus");
    }

    void cleanup()
    {
        m_kcm.reset();
        stopServer();
        m_dir.reset();
    }

    void loadsBothSlots()
    {
        begin();
        QTRY_VERIFY(m_kcm->isConfigured());
        QTRY_COMPARE(slot(1)->version(), u"0.2.13"_s);

        Kdos::SlotInfo *a = slot(0);
        Kdos::SlotInfo *b = slot(1);
        QCOMPARE(a->slotId(), u"a"_s);
        QCOMPARE(a->title(), u"KDOS 0.2.14 (slot A)"_s);
        QVERIFY(a->isRunning());
        QVERIFY(a->isConfirmed());
        QVERIFY(!a->isBootable());
        QCOMPARE(a->state(), u"confirmed"_s);
        QVERIFY(a->stateText().startsWith(u"Running now"_s));
        QCOMPARE(a->kernel(), u"6.17.4"_s);

        QCOMPARE(b->title(), u"KDOS 0.2.13 (slot B)"_s);
        QVERIFY(b->isPresent());
        QVERIFY(!b->isRunning());
        QVERIFY(b->isBootable());
        QCOMPARE(b->state(), u"installed"_s);
        QCOMPARE(b->buildTime().toSecsSinceEpoch(), 1790553600);
        QCOMPARE(b->treeCommit(), u"a81bec1c7d2e4f5a6b7c8d9e0f1a2b3c4d5e6f70"_s);
        QCOMPARE(b->shortCommit(), u"a81bec1c7d2e"_s);
        QVERIFY(!b->isEncrypted());

        QVERIFY(!m_kcm->isTrialArmed());
        QVERIFY(!m_kcm->isOnTrial());
        QVERIFY(m_kcm->rolledBackTitle().isEmpty());
        QCOMPARE(m_kcm->fallbackTitle(), u"KDOS 0.2.14 (slot A)"_s);
    }

    /* The 7.3.6 gate: "Start this version next time" on the previous
     * version arms a one-boot trial of it, in the boot state itself. */
    void startPreviousVersionArmsOneTrial()
    {
        begin();
        QTRY_VERIFY(slot(1)->isBootable());

        m_kcm->startNextTime(u"b"_s);
        QTRY_VERIFY(read(u"calls.log"_s).contains(u"Try b"_s));

        const QString st = read(u"bootstate"_s);
        QVERIFY2(st.contains(u"\ntry      = b\n"_s), qPrintable(st));
        QVERIFY2(st.contains(u"\nattempts = 1\n"_s), qPrintable(st));
        QVERIFY2(st.contains(u"\nactive   = a\n"_s), qPrintable(st));

        QTRY_VERIFY(m_kcm->isTrialArmed());
        QTRY_VERIFY(slot(1)->isTrial());
        QCOMPARE(m_kcm->trialTitle(), u"KDOS 0.2.13 (slot B)"_s);
        QCOMPARE(slot(1)->stateText(), u"Starts once at the next restart"_s);
        QVERIFY(m_kcm->message().isEmpty());
    }

    void cancelTrialDisarms()
    {
        begin();
        QTRY_VERIFY(slot(1)->isBootable());
        m_kcm->startNextTime(u"b"_s);
        QTRY_VERIFY(m_kcm->isTrialArmed());

        m_kcm->cancelTrial();
        QTRY_VERIFY(read(u"calls.log"_s).contains(u"CancelTry"_s));
        QVERIFY(read(u"bootstate"_s).contains(u"\ntry      = \n"_s));
        QTRY_VERIFY(!m_kcm->isTrialArmed());
        QTRY_VERIFY(!slot(1)->isTrial());
    }

    void notBootableIsRefusedBeforeTheBus()
    {
        begin();
        QTRY_VERIFY(slot(0)->isConfirmed());
        m_kcm->startNextTime(u"a"_s);
        QCOMPARE(m_kcm->messageType(), int(Kdos::SystemVersionsModule::Error));
        QCOMPARE(m_kcm->message(), u"This is already the version the computer starts."_s);
        QVERIFY(!read(u"calls.log"_s).contains(u"Try"_s));
    }

    void serverRefusalIsShownInItsOwnWords()
    {
        begin();
        QTRY_VERIFY(slot(0)->isConfirmed());
        m_kcm->client()->requestTry(u"a"_s);
        QTRY_COMPARE(m_kcm->messageType(), int(Kdos::SystemVersionsModule::Error));
        QCOMPARE(m_kcm->message(), u"bootctl: slot a is already active"_s);
        QVERIFY(read(u"bootstate"_s).contains(u"\ntry      = \n"_s));
    }

    void polkitRefusalSaysWhatWasNeeded()
    {
        begin();
        write(u"deny"_s, u"Try\nSetChannel\n"_s);
        QTRY_VERIFY(slot(1)->isBootable());
        m_kcm->startNextTime(u"b"_s);
        QTRY_COMPARE(m_kcm->messageType(), int(Kdos::SystemVersionsModule::Error));
        QVERIFY(m_kcm->message().contains(u"administrator's password"_s));
        QVERIFY(read(u"bootstate"_s).contains(u"\ntry      = \n"_s));
        QVERIFY(!m_kcm->isTrialArmed());
    }

    void channelIsASettingAppliedBySave()
    {
        begin();
        QTRY_VERIFY(m_kcm->isConfigured());
        QCOMPARE(m_kcm->channelKind(), u"release"_s);
        QVERIFY(!m_kcm->needsSave());
        QVERIFY(m_kcm->representsDefaults());

        m_kcm->setChannelKind(u"folder"_s);
        m_kcm->setChannelLocation(u"/run/media/kdos/KDOS-UPDATE"_s);
        QVERIFY(m_kcm->needsSave());
        QVERIFY(!m_kcm->representsDefaults());
        QVERIFY(m_kcm->channelError().isEmpty());

        m_kcm->save();
        QTRY_VERIFY(read(u"update.conf"_s).contains(u"tree = /run/media/kdos/KDOS-UPDATE\n"_s));
        QTRY_VERIFY(!m_kcm->needsSave());
        QCOMPARE(m_kcm->messageType(), int(Kdos::SystemVersionsModule::Positive));
        QTRY_COMPARE(m_kcm->savedChannelText(), u"/run/media/kdos/KDOS-UPDATE"_s);

        /* A second module reads it back as a folder. */
        Kdos::SystemVersionsModule again(nullptr, KPluginMetaData());
        again.load();
        QTRY_COMPARE(again.channelKind(), u"folder"_s);
        QCOMPARE(again.channelLocation(), u"/run/media/kdos/KDOS-UPDATE"_s);
        QVERIFY(!again.needsSave());

        /* Defaults are the KDOS releases, and Apply removes the line. */
        m_kcm->defaults();
        QCOMPARE(m_kcm->channelKind(), u"release"_s);
        QVERIFY(m_kcm->needsSave());
        m_kcm->save();
        QTRY_VERIFY(!read(u"update.conf"_s).contains(u"tree"_s));
        QTRY_VERIFY(!m_kcm->needsSave());
    }

    void invalidChannelIsNotWritten()
    {
        begin();
        QTRY_VERIFY(m_kcm->isConfigured());
        m_kcm->setChannelKind(u"address"_s);
        m_kcm->setChannelLocation(u"ftp://mirror.example/kdos"_s);
        QVERIFY(!m_kcm->channelError().isEmpty());
        m_kcm->save();
        QCOMPARE(m_kcm->messageType(), int(Kdos::SystemVersionsModule::Error));
        QVERIFY(m_kcm->needsSave());
        QTest::qWait(200);
        QVERIFY(!read(u"calls.log"_s).contains(u"SetChannel"_s));
    }

    void composeChannel_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("location");
        QTest::addColumn<QString>("channel");
        QTest::addColumn<bool>("ok");

        QTest::newRow("release") << u"release"_s << u"/ignored"_s << QString() << true;
        QTest::newRow("folder") << u"folder"_s << u" /media/stick "_s << u"/media/stick"_s << true;
        QTest::newRow("folder relative") << u"folder"_s << u"media/stick"_s << u"media/stick"_s << false;
        QTest::newRow("folder space") << u"folder"_s << u"/media/my stick"_s << u"/media/my stick"_s << false;
        QTest::newRow("folder empty") << u"folder"_s << QString() << QString() << false;
        QTest::newRow("https") << u"address"_s << u"https://example.org/kdos"_s << u"https://example.org/kdos"_s << true;
        QTest::newRow("http") << u"address"_s << u"http://10.0.0.2/kdos"_s << u"http://10.0.0.2/kdos"_s << true;
        QTest::newRow("file") << u"address"_s << u"file:///srv/kdos"_s << u"file:///srv/kdos"_s << true;
        QTest::newRow("ftp") << u"address"_s << u"ftp://example.org/kdos"_s << u"ftp://example.org/kdos"_s << false;
        QTest::newRow("no host") << u"address"_s << u"https:///kdos"_s << u"https:///kdos"_s << false;
        QTest::newRow("newline") << u"address"_s << u"https://a.example/x\ntree = /evil"_s << u"https://a.example/x\ntree = /evil"_s << false;
    }

    void composeChannel()
    {
        QFETCH(QString, kind);
        QFETCH(QString, location);
        QFETCH(QString, channel);
        QFETCH(bool, ok);
        QString err;
        QCOMPARE(Kdos::SystemVersionsModule::composeChannel(kind, location, &err), channel);
        QCOMPARE(err.isEmpty(), ok);
    }

    void runningOnTrialAwaitsTheBlessing()
    {
        begin();
        write(u"cmdline"_s, u"root=UUID=9b2e4d70-61c3-4f8a-8e22-7c5a1f0b3d02 kdos_slot=b\n"_s);
        QString st = read(u"bootstate"_s);
        st.replace(u"try      = \n"_s, u"try      = b\n"_s).replace(u"attempts = 0\n"_s, u"attempts = 1\n"_s);
        write(u"bootstate"_s, st);
        m_kcm->load();
        QTRY_VERIFY(m_kcm->isOnTrial());
        QVERIFY(!m_kcm->isTrialArmed());
        QVERIFY(m_kcm->isTrialConfirmedSoon());
        QTRY_VERIFY(slot(1)->isRunning());
        QCOMPARE(slot(1)->stateText(), u"On trial: kept once this session is confirmed"_s);

        /* A running trial is not called off from here. */
        m_kcm->cancelTrial();
        QTRY_COMPARE(m_kcm->messageType(), int(Kdos::SystemVersionsModule::Error));
        QVERIFY(m_kcm->message().contains(u"running on trial"_s));

        write(u"bless-graphical"_s, QString());
        m_kcm->load();
        QTRY_VERIFY(!m_kcm->isTrialConfirmedSoon());
        QVERIFY(m_kcm->isOnTrial());
    }

    void rollbackIsReported()
    {
        begin();
        write(u"rolledback"_s, u"b\n"_s);
        m_kcm->load();
        QTRY_VERIFY(slot(1)->isFailed());
        QCOMPARE(slot(1)->state(), u"failed"_s);
        QCOMPARE(m_kcm->rolledBackTitle(), u"KDOS 0.2.13 (slot B)"_s);
        QVERIFY(slot(1)->stateText().contains(u"went back"_s));
    }

    void updateJobIsShown()
    {
        begin();
        write(u"job"_s, u"acquire 3 12\n"_s);
        m_kcm->load();
        QTRY_COMPARE(m_kcm->jobProgress(), 25);
        QVERIFY(m_kcm->jobText().contains(u"3 of 12"_s));

        /* While the job runs, the offer is the job and an old error is
         * not the news. */
        write(u"offer"_s, u"0.2.15\n"_s);
        write(u"error"_s, u"kpkg: signature check failed\n"_s);
        m_kcm->load();
        QTRY_COMPARE(m_kcm->client()->state().value(u"error"_s).toString(), u"kpkg: signature check failed"_s);
        QCOMPARE(m_kcm->jobProgress(), 25);
        QVERIFY(m_kcm->offerVersion().isEmpty());
        QVERIFY(m_kcm->lastError().isEmpty());
    }

    void offerPreparedAndErrorAreShown()
    {
        begin();
        write(u"offer"_s, u"0.2.15\n"_s);
        m_kcm->load();
        QTRY_COMPARE(m_kcm->offerVersion(), u"0.2.15"_s);
        QVERIFY(!m_kcm->isUpdatePrepared());
        QVERIFY(m_kcm->lastError().isEmpty());

        /* Prepared in slot B and not installed: no trial is armed yet. */
        write(u"acquired"_s, u"b 0.2.15\n"_s);
        m_kcm->load();
        QTRY_VERIFY(m_kcm->isUpdatePrepared());
        QCOMPARE(m_kcm->preparedVersion(), u"0.2.15"_s);
        QVERIFY(!m_kcm->isTrialArmed());

        write(u"error"_s, u"kpkg: signature check failed\n"_s);
        m_kcm->load();
        QTRY_COMPARE(m_kcm->lastError(), u"kpkg: signature check failed"_s);
    }

    void oneRootIsNotConfigured()
    {
        begin();
        QFile::remove(path(u"bootstate"_s));
        m_kcm->load();
        QTRY_VERIFY(!m_kcm->isConfigured());
        QVERIFY(m_kcm->isAvailable());
        QTRY_VERIFY(!slot(0)->isPresent());
    }

    void serverComingAndGoing()
    {
        begin();
        QTRY_VERIFY(m_kcm->isConfigured());
        stopServer();
        QTRY_VERIFY(!m_kcm->isAvailable());
        QVERIFY(!m_kcm->isConfigured());
        QVERIFY(!m_kcm->needsSave());

        startServer();
        QTRY_VERIFY(m_kcm->isAvailable());
        QTRY_VERIFY(m_kcm->isConfigured());
        QTRY_COMPARE(slot(1)->version(), u"0.2.13"_s);
    }
};

QTEST_GUILESS_MAIN(SystemVersionsTest)

#include "systemversionstest.moc"
