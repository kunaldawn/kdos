/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   privacytest — the helper's lines, and the watch over a fake helper
 */

#include "privacystate.h"
#include "privacywatch.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Kdos;

class PrivacyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("PATH", QByteArray(QByteArray(KDOS_FAKE_KDOS_DIR) + ":/usr/bin:/bin"));
    }

    void parsesAllThreeKinds()
    {
        const PrivacyState s = kdosParsePrivacy(
            "{\"version\":1,"
            "\"mic\":[{\"id\":\"pw:57\",\"app\":\"Firefox\",\"icon\":\"firefox\",\"box\":\"www\",\"pid\":4242,\"muted\":false,\"mutable\":true,\"stoppable\":false},"
            "{\"id\":\"pw:60\",\"app\":\"Konsole\",\"box\":null,\"muted\":true,\"mutable\":true}],"
            "\"camera\":[{\"id\":\"video0:4242\",\"app\":\"Firefox\",\"box\":\"www\",\"pid\":4242,\"mutable\":true,\"stoppable\":true}],"
            "\"screen\":[{\"id\":\"portal:/org/freedesktop/portal/desktop/session/1_42/obs\",\"app\":\"OBS Studio\",\"stoppable\":true}]}\n");
        QVERIFY(s.valid);
        QCOMPARE(s.captures.size(), 4);
        QVERIFY(s.active(CaptureKind::Microphone));
        QVERIFY(s.active(CaptureKind::Camera));
        QVERIFY(s.active(CaptureKind::Screen));
        QVERIFY(!s.allMuted(CaptureKind::Microphone));

        const Capture &ff = s.captures[0];
        QCOMPARE(ff.id, QStringLiteral("pw:57"));
        QCOMPARE(ff.box, QStringLiteral("www"));
        QCOMPARE(ff.pid, 4242);
        QVERIFY(ff.canMute);
        QVERIFY(!ff.canStop);
        QCOMPARE(kdosCaptureLabel(ff), QStringLiteral("Firefox in box www"));
        QCOMPARE(kdosCaptureLabel(s.captures[1]), QStringLiteral("Konsole"));

        /* A camera is never "muted": the helper's mutable flag counts only for
         * the microphone. */
        QVERIFY(!s.captures[2].canMute);
        QVERIFY(s.captures[2].canStop);
        QCOMPARE(s.captures[3].kind, CaptureKind::Screen);
    }

    void allMutedNeedsACapture()
    {
        const PrivacyState idle = kdosParsePrivacy("{\"version\":1,\"mic\":[],\"camera\":[],\"screen\":[]}");
        QVERIFY(idle.valid);
        QVERIFY(!idle.allMuted(CaptureKind::Microphone));
        QVERIFY(!idle.active(CaptureKind::Microphone));
        const PrivacyState muted = kdosParsePrivacy("{\"version\":1,\"mic\":[{\"id\":\"a\",\"muted\":true},{\"id\":\"b\",\"muted\":true}]}");
        QVERIFY(muted.allMuted(CaptureKind::Microphone));
    }

    void refusesWhatIsNotState()
    {
        QVERIFY(!kdosParsePrivacy("").valid);
        QVERIFY(!kdosParsePrivacy("[]").valid);
        QVERIFY(!kdosParsePrivacy("{\"mic\":[]}").valid);
        QVERIFY(!kdosParsePrivacy("{\"version\":2,\"mic\":[]}").valid);
        QVERIFY(!kdosParsePrivacy("{\"version\":1,\"mic\":[").valid);

        const PrivacyState s = kdosParsePrivacy("{\"version\":1,\"mic\":[{\"app\":\"no id\"},{\"id\":\"--all\",\"app\":\"an option\"},{\"id\":\"ok\"}]}");
        QVERIFY(s.valid);
        QCOMPARE(s.captures.size(), 1);
        QCOMPARE(s.captures[0].id, QStringLiteral("ok"));
        QCOMPARE(kdosCaptureLabel(s.captures[0]), QStringLiteral("An unnamed program"));
    }

    void longFieldsAreCut()
    {
        const QByteArray big(4000, 'x');
        const PrivacyState s = kdosParsePrivacy("{\"version\":1,\"screen\":[{\"id\":\"s\",\"app\":\"" + big + "\"}]}");
        QCOMPARE(s.captures.size(), 1);
        QCOMPARE(s.captures[0].app.size(), 512);
    }

    void actionArguments()
    {
        QCOMPARE(kdosMuteArguments(QStringLiteral("pw:57"), true), QStringList({QStringLiteral("privacy"), QStringLiteral("mute"), QStringLiteral("pw:57")}));
        QCOMPARE(kdosMuteArguments(QStringLiteral("pw:57"), false), QStringList({QStringLiteral("privacy"), QStringLiteral("unmute"), QStringLiteral("pw:57")}));
        QCOMPARE(kdosStopArguments(QStringLiteral("video0:1")), QStringList({QStringLiteral("privacy"), QStringLiteral("stop"), QStringLiteral("video0:1")}));
        Capture c;
        c.kind = CaptureKind::Camera;
        c.id = QStringLiteral("x");
        c.app = QStringLiteral("Cheese");
        c.box = QStringLiteral("cam");
        const QVariantMap m = kdosCaptureMap(c);
        QCOMPARE(m.value(QStringLiteral("kind")).toString(), QStringLiteral("camera"));
        QCOMPARE(m.value(QStringLiteral("label")).toString(), QStringLiteral("Cheese in box cam"));
    }

    void watchFollowsTheHelper()
    {
        qputenv("KDOS_FAKE_MODE", "stream");
        PrivacyWatch w;
        QSignalSpy spy(&w, &PrivacyWatch::changed);
        w.start();

        QTRY_VERIFY_WITH_TIMEOUT(w.available() && w.state().active(CaptureKind::Microphone), 5000);
        QCOMPARE(w.state().captures[0].box, QStringLiteral("www"));

        /* The second line arrives in two writes; it counts only once whole. */
        QTRY_VERIFY_WITH_TIMEOUT(w.state().active(CaptureKind::Camera), 5000);
        QVERIFY(!w.state().active(CaptureKind::Microphone));

        /* A line of an unknown version changes nothing; the helper's exit
         * darkens the lamps and says it is being restarted. */
        QTRY_VERIFY_WITH_TIMEOUT(!w.available(), 6000);
        QVERIFY(!w.state().active(CaptureKind::Camera));
        QVERIFY(!w.problem().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(w.available() && w.state().active(CaptureKind::Microphone), 6000);
    }

    void helperWithoutTheVerbIsNotRestarted()
    {
        qputenv("KDOS_FAKE_MODE", "usage");
        PrivacyWatch w;
        QSignalSpy spy(&w, &PrivacyWatch::changed);
        w.start();
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
        QVERIFY(!w.available());
        QCOMPARE(w.problem(), QStringLiteral("kdos: unknown command 'privacy'"));
        QTest::qWait(2500);
        QCOMPARE(spy.count(), 1);
    }

    void actionsRunKdos()
    {
        QTemporaryDir t;
        const QString log = t.filePath(QStringLiteral("log"));
        qputenv("KDOS_FAKE_LOG", QFile::encodeName(log));
        PrivacyWatch w;
        QVERIFY(w.run(kdosStopArguments(QStringLiteral("video0:42"))));
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(log), 5000);
        QFile f(log);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("privacy stop video0:42\n"));
    }

    void noKdosNoLamps()
    {
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", "/nonexistent");
        PrivacyWatch w;
        if (!PrivacyWatch::program().isEmpty()) {
            qputenv("PATH", path);
            QSKIP("a kdos is installed in /usr/local/bin on this machine");
        }
        w.start();
        QVERIFY(!w.available());
        QVERIFY(!w.problem().isEmpty());
        QVERIFY(!w.run({QStringLiteral("privacy")}));
        qputenv("PATH", path);
    }
};

QTEST_GUILESS_MAIN(PrivacyTest)

#include "privacytest.moc"
