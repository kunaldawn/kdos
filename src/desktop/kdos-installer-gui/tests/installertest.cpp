/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   installertest — the answer file, the event stream and the layout list
 *
 * The fixtures under fixtures/ are kinstall's own output: two streams a real
 * `kinstall --events json` wrote (refused answers; a dry run stopped for not
 * being root) and one success in the same shape. Beside them, a rules list,
 * an XKB symbols directory holding `us` and `de` only, and a zone1970.tab.
 */

#include "answers.h"
#include "backend.h"
#include "eventstream.h"
#include "keyboardmodel.h"

#include <QFile>
#include <QSignalSpy>
#include <QTest>

using namespace Kdos;

class InstallerTest : public QObject
{
    Q_OBJECT

private:
    static QByteArray fixture(const char *name)
    {
        QFile f(QStringLiteral(FIXTURES_DIR "/") + QLatin1String(name));
        if (!f.open(QIODevice::ReadOnly)) {
            qFatal("missing fixture %s", name);
        }
        return f.readAll();
    }

    static QMap<QString, QString> parse(const QString &text)
    {
        QMap<QString, QString> m;
        for (const QString &l : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const qsizetype eq = l.indexOf(QLatin1Char('='));
            if (eq <= 0) {
                qFatal("not a key = value line: %s", qPrintable(l));
            }
            const QString k = l.left(eq).trimmed();
            if (m.contains(k)) {
                qFatal("key twice: %s", qPrintable(k));
            }
            m.insert(k, l.mid(eq + 1).trimmed());
        }
        return m;
    }

private Q_SLOTS:
    void answerFile()
    {
        Answers a;
        a.setProperty("disk", QStringLiteral("/dev/vda"));
        a.setProperty("username", QStringLiteral("ann"));
        a.setProperty("fullname", QStringLiteral("Ann Lee"));
        a.setProperty("password", QStringLiteral("s3cret\nroot_password = x"));
        a.setProperty("xkbLayout", QStringLiteral("de"));
        a.setProperty("theme", QStringLiteral("amber"));
        a.setProperty("desk", QStringLiteral("tiling"));
        const auto m = parse(a.toText());
        QCOMPARE(m.value(QStringLiteral("disk")), QStringLiteral("/dev/vda"));
        QCOMPARE(m.value(QStringLiteral("plan")), QStringLiteral("wipe"));
        QCOMPARE(m.value(QStringLiteral("lvm")), QStringLiteral("1"));
        QCOMPARE(m.value(QStringLiteral("xkb_layout")), QStringLiteral("de"));
        QVERIFY(!m.contains(QStringLiteral("keymap")));
        QCOMPARE(m.value(QStringLiteral("theme")), QStringLiteral("amber"));
        QCOMPARE(m.value(QStringLiteral("desk")), QStringLiteral("tiling"));
        QCOMPARE(m.value(QStringLiteral("reboot")), QStringLiteral("0"));
        QCOMPARE(m.value(QStringLiteral("apps")), QStringLiteral("none"));
        /* A line break in a value is flattened, never a second key. */
        QVERIFY(!m.contains(QStringLiteral("root_password")));
        QCOMPARE(m.value(QStringLiteral("autologin")), QStringLiteral("no"));

        a.setProperty("autologin", true);
        QCOMPARE(parse(a.toText()).value(QStringLiteral("autologin")), QStringLiteral("no"));
        a.setProperty("luks", true);
        a.setProperty("luksPassphrase", QStringLiteral("pp"));
        QCOMPARE(parse(a.toText()).value(QStringLiteral("autologin")), QStringLiteral("yes"));
        QCOMPARE(parse(a.toText()).value(QStringLiteral("luks_passphrase")), QStringLiteral("pp"));

        a.setProperty("oem", true);
        const auto o = parse(a.toText());
        QCOMPARE(o.value(QStringLiteral("oem")), QStringLiteral("1"));
        QVERIFY(!o.contains(QStringLiteral("username")));
        QVERIFY(!o.contains(QStringLiteral("password")));

        a.setAppChosen(QStringLiteral("office"), true);
        a.setAppChosen(QStringLiteral("dev"), true);
        a.setAppChosen(QStringLiteral("office"), false);
        QCOMPARE(parse(a.toText()).value(QStringLiteral("apps")), QStringLiteral("dev"));
    }

    void names()
    {
        QCOMPARE(Answers::suggestUsername(QStringLiteral("  José Álvarez")), QStringLiteral("jose"));
        QCOMPARE(Answers::suggestUsername(QStringLiteral("2pac Shakur")), QStringLiteral("pac"));
        QVERIFY(Answers::usernameOk(QStringLiteral("ann")));
        QVERIFY(!Answers::usernameOk(QStringLiteral("Ann")));
        QVERIFY(!Answers::usernameOk(QStringLiteral("9lives")));
        QVERIFY(Answers::hostnameOk(QStringLiteral("kdos-box1")));
        QVERIFY(!Answers::hostnameOk(QStringLiteral("-kdos")));
        QVERIFY(!Answers::hostnameOk(QStringLiteral("a.b")));
        QVERIFY(Answers::fullnameOk(QStringLiteral("José Álvarez, Room 4")));
        QVERIFY(!Answers::fullnameOk(QStringLiteral("Ann:0:0")));
        QVERIFY(!Answers::fullnameOk(QStringLiteral("Ann\tLee")));
        QVERIFY(Answers::fitsAnswerFile(QStringLiteral("correct horse")));
        QVERIFY(!Answers::fitsAnswerFile(QStringLiteral(" leading")));
        QVERIFY(!Answers::fitsAnswerFile(QStringLiteral("trailing ")));
        QVERIFY(!Answers::fitsAnswerFile(QStringLiteral("two\nlines")));

        Answers a;
        a.setReservedNames({QStringLiteral("root"), QStringLiteral("sddm")});
        QVERIFY(a.usernameReserved(QStringLiteral("root")));
        QVERIFY(a.usernameReserved(QStringLiteral("sddm")));
        QVERIFY(!a.usernameReserved(QStringLiteral("ann")));
    }

    void streamSuccess()
    {
        EventStream s;
        QSignalSpy done(&s, &EventStream::finished);
        const QByteArray bytes = fixture("success.jsonl");
        /* In pieces that cut lines, as a pipe delivers them. */
        for (qsizetype i = 0; i < bytes.size(); i += 7) {
            QVERIFY(s.feed(bytes.mid(i, 7)));
        }
        QCOMPARE(done.count(), 1);
        QCOMPARE(s.status(), 0);
        QCOMPARE(s.rowCount(), 5);
        for (int i = 0; i < s.rowCount(); i++) {
            const QString st = s.data(s.index(i), EventStream::StateRole).toString();
            QVERIFY2(st == (i == 3 ? QLatin1String("skipped") : QLatin1String("done")), qPrintable(st));
        }
        QCOMPARE(s.warnings().size(), 1);
        QCOMPARE(s.log().size(), 1);
        QCOMPARE(s.overall(), 1.0);
        QVERIFY(s.failure().isEmpty());
        s.close(0);
        QCOMPARE(done.count(), 1);
    }

    void streamRefused()
    {
        EventStream s;
        QVERIFY(s.feed(fixture("refused-answers.jsonl")));
        QCOMPARE(s.status(), 2);
        QVERIFY(s.failure().contains(QLatin1String("encrypted root")));

        EventStream r;
        QVERIFY(r.feed(fixture("refused-not-root.jsonl")));
        QCOMPARE(r.status(), 1);
        QVERIFY(r.failure().contains(QLatin1String("root")));
        QCOMPARE(r.data(r.index(0), EventStream::StateRole).toString(), QStringLiteral("failed"));
    }

    void streamRefusedPermission()
    {
        /* pkexec's 127 is "not authorised" as well as its 126 "dismissed";
         * before any event both are a refusal, never a missing kinstall. */
        for (int code : {126, 127}) {
            EventStream s;
            QSignalSpy done(&s, &EventStream::finished);
            s.close(code);
            QCOMPARE(done.count(), 1);
            QVERIFY(!s.started());
            QCOMPARE(s.failure(), QStringLiteral("Permission to install was not given."));
        }
        EventStream f;
        f.fail(QStringLiteral("kinstall is not installed"));
        QCOMPARE(f.status(), 1);
        QVERIFY(!f.started());
        QCOMPARE(f.failure(), QStringLiteral("kinstall is not installed"));
    }

    void streamDied()
    {
        EventStream s;
        const QByteArray bytes = fixture("success.jsonl");
        /* The plan line and part of the next: kinstall had started. */
        QVERIFY(s.feed(bytes.left(bytes.indexOf('\n') + 20)));
        QCOMPARE(s.status(), -1);
        QVERIFY(s.started());
        s.close(126);
        QCOMPARE(s.status(), 126);
        QVERIFY(s.failure().contains(QLatin1String("stopped unexpectedly")));
        QVERIFY(!s.feed(QByteArrayLiteral("not json\n")));
    }

    void keyboards()
    {
        KeyboardModel k;
        k.parse(fixture("evdev.lst"));
        QCOMPARE(k.rowCount(), 3);
        QCOMPARE(k.data(k.index(0), KeyboardModel::DescriptionRole).toString(), QStringLiteral("English (UK)"));
        QCOMPARE(k.description(QStringLiteral("de")), QStringLiteral("German"));
        const QStringList v = k.variants(QStringLiteral("de"));
        QCOMPARE(v.size(), 6);
        QCOMPARE(v.at(0), QString());
        QCOMPARE(v.at(2), QStringLiteral("nodeadkeys"));
        QCOMPARE(k.variants(QStringLiteral("gb")).size(), 2);
        QCOMPARE(k.indexOf(QStringLiteral("pc105")), -1);
        QCOMPARE(k.variantDescription(QStringLiteral("de"), QStringLiteral("nodeadkeys")), QStringLiteral("German (no dead keys)"));
        QCOMPARE(k.variantDescription(QStringLiteral("de"), QString()), QString());
        QCOMPARE(k.variantDescription(QStringLiteral("de"), QStringLiteral("odd")), QStringLiteral("odd"));

        /* A layout with no symbols file is not offered. */
        QByteArray lst = fixture("evdev.lst");
        lst.replace("! layout\n", "! layout\n  custom          A user-defined custom Layout\n");
        KeyboardModel s;
        s.parse(lst, QStringLiteral(FIXTURES_DIR "/xkb/symbols"));
        QCOMPARE(s.rowCount(), 2);
        QCOMPARE(s.indexOf(QStringLiteral("custom")), -1);
        QCOMPARE(s.indexOf(QStringLiteral("gb")), -1);
        QVERIFY(s.indexOf(QStringLiteral("us")) >= 0);
    }

    void zones()
    {
        const QStringList z = Backend::readZones(QStringLiteral(FIXTURES_DIR "/zone1970.tab"));
        QCOMPARE(z, (QStringList{QStringLiteral("UTC"), QStringLiteral("Africa/Nairobi"), QStringLiteral("America/New_York"),
                                 QStringLiteral("Asia/Kolkata"), QStringLiteral("Europe/Berlin")}));
        QCOMPARE(Backend::readZones(QStringLiteral(FIXTURES_DIR "/absent.tab")), QStringList{QStringLiteral("UTC")});
    }
};

QTEST_GUILESS_MAIN(InstallerTest)

#include "installertest.moc"
