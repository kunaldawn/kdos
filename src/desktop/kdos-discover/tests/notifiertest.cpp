/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — KdosNotifier, with and without kdos-sysupdated
 *
 * Without the daemon the tray reads update.json and asks `kdos-bootctl
 * status --json`; with it, io.kdos.Slots1's State decides, served here on a
 * private session bus by an object of this test. `kdos` and `kdos-bootctl`
 * are scripts on PATH printing fixtures.
 * ---------------------------------
 */

#include "KdosNotifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace Kdos;

// io.kdos.Slots1 as kdos-sysupdated serves it, reduced to its State.
class FakeSlots : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.kdos.Slots1")
    Q_PROPERTY(QVariantMap State READ state)
public:
    QVariantMap state() const
    {
        return m_state;
    }
    void setState(const QVariantMap &s)
    {
        m_state = s;
        QDBusMessage changed = QDBusMessage::createSignal(u"/io/kdos/Slots1"_s, u"org.freedesktop.DBus.Properties"_s, u"PropertiesChanged"_s);
        changed << u"io.kdos.Slots1"_s << QVariantMap{{u"State"_s, QVariant::fromValue(m_state)}} << QStringList();
        QDBusConnection::sessionBus().send(changed);
    }

private:
    QVariantMap m_state;
};

class NotifierTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void withoutTheDaemon();
    void withTheDaemon();

private:
    QByteArray fixture(const QString &name);
    void putDocument(const QString &fixtureName);
    QVariantMap jsonMap(const QString &fixtureName);

    QTemporaryDir m_dir;
    QString m_fixtures;
};

QByteArray NotifierTest::fixture(const QString &name)
{
    QFile f(m_fixtures + u'/' + name);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", qPrintable(name));
    return f.readAll();
}

QVariantMap NotifierTest::jsonMap(const QString &fixtureName)
{
    return QJsonDocument::fromJson(fixture(fixtureName)).object().toVariantMap();
}

// Renamed into place whole, as `kdos update check --out` writes it.
void NotifierTest::putDocument(const QString &fixtureName)
{
    QSaveFile f(m_dir.filePath(u"lib/update.json"_s));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(fixture(fixtureName));
    QVERIFY(f.commit());
}

void NotifierTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_fixtures = QFileInfo(QFINDTESTDATA("fixtures/cve.json")).absolutePath();
    QVERIFY(QDir().mkpath(m_dir.filePath(u"bin"_s)));
    QVERIFY(QDir().mkpath(m_dir.filePath(u"lib"_s)));
    const QList<std::pair<QString, QByteArray>> tools{
        {u"kdos"_s, "[ \"$1 $2\" = \"cve --json\" ] && cat \"$KDOS_TEST_FIXTURES/cve.json\" && exit 1\n"},
        {u"kdos-bootctl"_s, "[ \"$1 $2\" = \"status --json\" ] && cat \"$KDOS_TEST_FIXTURES/bootctl-armed.json\"\n"},
    };
    for (const auto &[name, body] : tools) {
        QFile f(m_dir.filePath(u"bin/"_s + name));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("#!/bin/sh\n" + body + "exit 0\n");
        f.close();
        QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
    qputenv("PATH", (m_dir.filePath(u"bin"_s) + u':' + qEnvironmentVariable("PATH")).toUtf8());
    qputenv("KDOS_TEST_FIXTURES", m_fixtures.toUtf8());
    qputenv("KDOS_UPDATE_DOC", m_dir.filePath(u"lib/update.json"_s).toUtf8());
}

void NotifierTest::withoutTheDaemon()
{
    // A bus with no io.kdos.Slots1 on it, or no bus: bootctl answers.
    qputenv("KDOS_SLOTS1_BUS", "session");
    putDocument(u"update-offer.json"_s);
    KdosNotifier n;
    QSignalSpy reboot(&n, &BackendNotifierModule::needsRebootChanged);
    n.recheckSystemUpdateNeeded();
    QTRY_VERIFY(n.hasUpdates());
    // curl's finding is closed by the update; that is a security update.
    QTRY_VERIFY(n.hasSecurityUpdates());
    QTRY_VERIFY(n.needsReboot());
    QCOMPARE(reboot.count(), 1);

    // A new document, renamed into place, is read without a recheck.
    putDocument(u"update-none.json"_s);
    QTRY_VERIFY_WITH_TIMEOUT(!n.hasUpdates(), 5000);
    QVERIFY(!n.hasSecurityUpdates());
}

void NotifierTest::withTheDaemon()
{
    if (!QDBusConnection::sessionBus().isConnected())
        QSKIP("no session bus to serve io.kdos.Slots1 on");
    qputenv("KDOS_SLOTS1_BUS", "session");
    FakeSlots slots;
    slots.setState(jsonMap(u"slots-state-acquiring.json"_s));
    QVERIFY(QDBusConnection::sessionBus().registerObject(u"/io/kdos/Slots1"_s, &slots, QDBusConnection::ExportAllProperties));
    QVERIFY(QDBusConnection::sessionBus().registerService(u"io.kdos.Slots1"_s));

    // update.json offers nothing; the daemon's answer is the one that counts.
    putDocument(u"update-none.json"_s);
    KdosNotifier n;
    QSignalSpy reboot(&n, &BackendNotifierModule::needsRebootChanged);
    n.recheckSystemUpdateNeeded();
    QTRY_VERIFY(n.hasUpdates());
    QVERIFY(!n.needsReboot());

    // Install armed the slot: the offer is gone and a restart is pending,
    // announced by PropertiesChanged alone.
    slots.setState(jsonMap(u"slots-state-armed.json"_s));
    QTRY_VERIFY(n.needsReboot());
    QTRY_VERIFY(!n.hasUpdates());
    QCOMPARE(reboot.count(), 1);

    QDBusConnection::sessionBus().unregisterService(u"io.kdos.Slots1"_s);
    QDBusConnection::sessionBus().unregisterObject(u"/io/kdos/Slots1"_s);
}

int main(int argc, char **argv)
{
    // The job tracker the plugin hands an acquiring update to is a widgets
    // library's; it runs under a QApplication, as Discover's notifier does.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    NotifierTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "notifiertest.moc"
