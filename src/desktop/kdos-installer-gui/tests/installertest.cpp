/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   installertest — the answer file, the event stream, the layout list and
 *   the password pairs' keyboard path
 *
 * The fixtures under fixtures/ are kinstall's own output: two streams a real
 * `kinstall --events json` wrote (refused answers; a dry run stopped for not
 * being root) and one success in the same shape. Beside them, a rules list,
 * an XKB symbols directory holding `us` and `de` only, a zone1970.tab, and
 * two mounts tables, the live medium's and an installed machine's.
 *
 * The Encryption and Account pages are loaded from the source tree's qml/,
 * with a Backend that holds the answers and nothing else, in a window of the
 * installer's default size, and driven with keys.
 */

#include "answers.h"
#include "backend.h"
#include "eventstream.h"
#include "keyboardmodel.h"

#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

using namespace Kdos;

/* The pages' i18n() and i18nc(), as KLocalizedContext gives them: the text
 * with its arguments filled in, untranslated. */
class PageStrings : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QString i18n(const QString &text, const QVariant &a = {}, const QVariant &b = {}) const
    {
        return fill(text, a, b);
    }
    Q_INVOKABLE QString i18nc(const QString &, const QString &text, const QVariant &a = {}, const QVariant &b = {}) const
    {
        return fill(text, a, b);
    }

private:
    static QString fill(QString text, const QVariant &a, const QVariant &b)
    {
        if (a.isValid()) {
            text = text.arg(a.toString());
        }
        if (b.isValid()) {
            text = text.arg(b.toString());
        }
        return text;
    }
};

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

    static void write(const QString &path, const QByteArray &text)
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(text) != text.size()) {
            qFatal("cannot write %s", qPrintable(path));
        }
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

        /* Native groups are their own key: an id the catalogue shares
         * (office) names the native group there, never a boxed one. */
        QCOMPARE(parse(a.toText()).value(QStringLiteral("groups")), QStringLiteral("none"));
        a.setGroupChosen(QStringLiteral("office"), true);
        a.setGroupChosen(QStringLiteral("games"), true);
        a.setGroupChosen(QStringLiteral("games"), false);
        QCOMPARE(parse(a.toText()).value(QStringLiteral("groups")), QStringLiteral("office"));
        QCOMPARE(parse(a.toText()).value(QStringLiteral("apps")), QStringLiteral("dev"));

        /* The language group a keyboard points at is ticked once; an
         * untick of it survives the next dump, a different keyboard moves
         * the tick, and a manual tick nobody suggested is left alone. */
        a.suggestGroups({QStringLiteral("lang-ja")});
        QCOMPARE(parse(a.toText()).value(QStringLiteral("groups")), QStringLiteral("office lang-ja"));
        a.setGroupChosen(QStringLiteral("lang-ja"), false);
        a.suggestGroups({QStringLiteral("lang-ja")});
        QCOMPARE(parse(a.toText()).value(QStringLiteral("groups")), QStringLiteral("office"));
        a.suggestGroups({QStringLiteral("lang-ko")});
        QCOMPARE(parse(a.toText()).value(QStringLiteral("groups")), QStringLiteral("office lang-ko"));
        a.suggestGroups({});
        QCOMPARE(parse(a.toText()).value(QStringLiteral("groups")), QStringLiteral("office"));
    }

    void names()
    {
        QCOMPARE(Answers::suggestUsername(QStringLiteral("  José Álvarez")), QStringLiteral("jose"));
        QCOMPARE(Answers::suggestUsername(QStringLiteral("2pac Shakur")), QStringLiteral("pac"));
        QVERIFY(Answers::usernameOk(QStringLiteral("ann")));
        QVERIFY(!Answers::usernameOk(QStringLiteral("Ann")));
        QVERIFY(!Answers::usernameOk(QStringLiteral("9lives")));
        QVERIFY(!Answers::usernameOk(QStringLiteral("-ann")));
        QVERIFY(Answers::usernameOk(QStringLiteral("_ann-")));
        QVERIFY(!Answers::usernameOk(QString(33, QLatin1Char('a'))));
        QCOMPARE(Answers::suggestUsername(QStringLiteral("-2-Ann")), QStringLiteral("ann"));
        QVERIFY(Answers::hostnameOk(QStringLiteral("kdos-box1")));
        QVERIFY(!Answers::hostnameOk(QStringLiteral("-kdos")));
        QVERIFY(!Answers::hostnameOk(QStringLiteral("a.b")));
        QVERIFY(!Answers::hostnameOk(QStringLiteral("kdos-")));
        QVERIFY(!Answers::hostnameOk(QString()));
        QVERIFY(Answers::hostnameOk(QStringLiteral("k")));
        QVERIFY(Answers::hostnameOk(QString(63, QLatin1Char('k'))));
        QVERIFY(!Answers::hostnameOk(QString(64, QLatin1Char('k'))));
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

    void streamWeights()
    {
        /* Weights in tenths of a second as kinstall writes them: Prepare
         * 0.3 s and Finish 0.3 s against a 54 s copy, and the skipped slot
         * B's 26 s counts for nothing. Half way through the copy the bar is
         * at (3 + 270) / 546 = 0.5, where counting steps would put it at
         * 1.5 / 3, and at Finish it is at 543 / 546. */
        EventStream s;
        QVERIFY(s.feed(QByteArrayLiteral(
            "{\"t\":\"plan\",\"steps\":["
            "{\"index\":0,\"title\":\"Prepare\",\"detail\":\"\",\"state\":\"pending\",\"weight\":3},"
            "{\"index\":1,\"title\":\"Copy system\",\"detail\":\"\",\"state\":\"pending\",\"weight\":540},"
            "{\"index\":2,\"title\":\"Slot B\",\"detail\":\"\",\"state\":\"skipped\",\"weight\":260},"
            "{\"index\":3,\"title\":\"Finish\",\"detail\":\"\",\"state\":\"pending\",\"weight\":3}]}\n"
            "{\"t\":\"S\",\"step\":0,\"title\":\"Prepare\"}\n"
            "{\"t\":\"S\",\"step\":1,\"title\":\"Copy system\"}\n")));
        QCOMPARE(s.overall(), 3.0 / 546);
        QVERIFY(s.feed(QByteArrayLiteral("{\"t\":\"P\",\"step\":1,\"frac\":0.5}\n")));
        QCOMPARE(s.overall(), 0.5);
        QVERIFY(s.feed(QByteArrayLiteral("{\"t\":\"S\",\"step\":3,\"title\":\"Finish\"}\n")));
        QCOMPARE(s.overall(), 543.0 / 546);

        /* A plan line with no weights, as an older kinstall writes it,
         * counts every step as 1. */
        EventStream u;
        QVERIFY(u.feed(fixture("success.jsonl").left(fixture("success.jsonl").indexOf('\n') + 1)));
        QVERIFY(u.feed(QByteArrayLiteral("{\"t\":\"S\",\"step\":2,\"title\":\"Copy system\"}\n"
                                         "{\"t\":\"P\",\"step\":2,\"frac\":0.5}\n")));
        QCOMPARE(u.overall(), 0.5 / 4);
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

    void live()
    {
        /* An overlay at `/` is the live medium; one under a container's
         * store is not, and an unreadable table is not either. */
        QVERIFY(Backend::liveRoot(QStringLiteral(FIXTURES_DIR "/mounts-live")));
        QVERIFY(!Backend::liveRoot(QStringLiteral(FIXTURES_DIR "/mounts-installed")));
        QVERIFY(!Backend::liveRoot(QStringLiteral(FIXTURES_DIR "/absent.mounts")));
    }

    void diskPlan()
    {
        /* A disk with no partitions leaves the reuse plan nothing to use,
         * so choosing one turns to the erase plan; any other keeps it. */
        QCOMPARE(Backend::diskPlan(QStringLiteral("reuse"), 0), QStringLiteral("wipe"));
        QCOMPARE(Backend::diskPlan(QStringLiteral("wipe"), 0), QStringLiteral("wipe"));
        QCOMPARE(Backend::diskPlan(QStringLiteral("reuse"), 2), QStringLiteral("reuse"));
        QCOMPARE(Backend::diskPlan(QStringLiteral("wipe"), 2), QStringLiteral("wipe"));
    }

    void dumpArgs()
    {
        /* Root's payload goes to every plan, root's or not, and to a probe
         * as this user, so the Disk page's plan decides on the Summary's
         * number; a root probe takes none and measures again, the dumps
         * that size nothing take none, and before a root probe there is
         * none. */
        const QVariantMap root{{QStringLiteral("payload_kb"), 4963328.0}, {QStringLiteral("appbox_kb"), 2048.0}};
        const QStringList payload{QStringLiteral("--payload-kb"), QStringLiteral("4963328"), QStringLiteral("--appbox-kb"),
                                  QStringLiteral("2048")};
        const QStringList answers{QStringLiteral("--config"), QStringLiteral("-")};
        const QStringList plan{QStringLiteral("--dump"), QStringLiteral("plan"), QStringLiteral("--json")};
        const QStringList probe{QStringLiteral("--dump"), QStringLiteral("probe"), QStringLiteral("--json")};
        QCOMPARE(Backend::dumpArgs(QStringLiteral("plan"), true, root), answers + payload + plan);
        QCOMPARE(Backend::dumpArgs(QStringLiteral("probe"), true, root), answers + payload + probe);
        QCOMPARE(Backend::dumpArgs(QStringLiteral("plan"), true, root, true), answers + payload + plan);
        QCOMPARE(Backend::dumpArgs(QStringLiteral("probe"), true, root, true), answers + probe);
        QCOMPARE(Backend::dumpArgs(QStringLiteral("plan"), true, {}), answers + plan);
        QCOMPARE(Backend::dumpArgs(QStringLiteral("look"), false, root),
                 (QStringList{QStringLiteral("--dump"), QStringLiteral("look"), QStringLiteral("--json")}));
        QCOMPARE(Backend::dumpArgs(QStringLiteral("apps"), true, root),
                 (answers + QStringList{QStringLiteral("--dump"), QStringLiteral("apps"), QStringLiteral("--json")}));
    }

    /* Every QML file in qml/ is in the module's QML_FILES: a component the
     * module does not carry is an unknown type at run time, and the page
     * that uses it fails to load. */
    void moduleListsEveryFile()
    {
        QFile cmake(QStringLiteral(FIXTURES_DIR "/../../CMakeLists.txt"));
        QVERIFY(cmake.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(cmake.readAll());
        const QStringList files = QDir(QStringLiteral(FIXTURES_DIR "/../../qml")).entryList({QStringLiteral("*.qml")}, QDir::Files);
        QVERIFY(files.contains(QStringLiteral("Main.qml")));
        for (const QString &f : files) {
            QVERIFY2(text.contains(QStringLiteral("qml/") + f), qPrintable(QStringLiteral("qml/%1 is not in qt_add_qml_module's QML_FILES").arg(f)));
        }
    }

    void revealByKeyboard_data()
    {
        QTest::addColumn<QString>("page");
        QTest::addColumn<int>("pair");
        QTest::addColumn<int>("units");
        QTest::newRow("encryption") << QStringLiteral("EncryptionPage") << 0 << 58;
        QTest::newRow("account") << QStringLiteral("AccountPage") << 0 << 58;
        QTest::newRow("root") << QStringLiteral("AccountPage") << 1 << 58;
        QTest::newRow("encryption, narrowest window") << QStringLiteral("EncryptionPage") << 0 << 40;
        QTest::newRow("account, narrowest window") << QStringLiteral("AccountPage") << 0 << 40;
    }

    /* Type, Tab, type, Shift+Tab, Space: what is typed after the Tab lands
     * in the second field, Shift+Tab from it stops on the first field's
     * reveal button, and Space there shows the first field's text and types
     * nothing into it. The page sits beside an 11-grid-unit step list, as
     * in the installer's window. At the default width of 58 grid units the
     * short advice under the first field sits on one line; at the minimum
     * of 40 the form and its advice stay inside the page. */
    void revealByKeyboard()
    {
        QFETCH(QString, page);
        QFETCH(int, pair);
        QFETCH(int, units);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mod = dir.filePath(QStringLiteral("org/kdos/installer"));
        QVERIFY(QDir().mkpath(mod));
        const QString src = QStringLiteral(FIXTURES_DIR "/../../qml/");
        for (const char *f : {"WizardPage.qml", "StrengthLabel.qml", "EncryptionPage.qml", "AccountPage.qml"}) {
            QVERIFY(QFile::copy(src + QLatin1String(f), mod + QLatin1Char('/') + QLatin1String(f)));
        }
        write(mod + QStringLiteral("/qmldir"),
              "module org.kdos.installer\n"
              "WizardPage 1.0 WizardPage.qml\n"
              "StrengthLabel 1.0 StrengthLabel.qml\n"
              "EncryptionPage 1.0 EncryptionPage.qml\n"
              "AccountPage 1.0 AccountPage.qml\n"
              "singleton Backend 1.0 Backend.qml\n");
        write(mod + QStringLiteral("/Backend.qml"),
              "pragma Singleton\n"
              "import QtQuick\n"
              "QtObject {\n"
              "    property QtObject answers: QtObject {\n"
              "        property bool luks: true\n"
              "        property string plan: \"wipe\"\n"
              "        property bool autologin: false\n"
              "        property bool autologinLock: false\n"
              "        property bool oem: false\n"
              "        property bool admin: true\n"
              "        property bool rootLocked: false\n"
              "        property string hostname: \"kdos\"\n"
              "        property string username: \"ann\"\n"
              "        property string fullname\n"
              "        property string password\n"
              "        property string rootPassword\n"
              "        property string luksPassphrase\n"
              "        function fitsAnswerFile(s) { return true; }\n"
              "        function hostnameOk(s) { return true; }\n"
              "        function fullnameOk(s) { return true; }\n"
              "        function usernameOk(s) { return true; }\n"
              "        function usernameReserved(s) { return false; }\n"
              "        function suggestUsername(s) { return \"ann\"; }\n"
              "    }\n"
              "}\n");
        write(dir.filePath(QStringLiteral("main.qml")),
              "import QtQuick\n"
              "import QtQuick.Window\n"
              "import org.kde.kirigami as Kirigami\n"
              "import org.kdos.installer\n"
              "Window {\n"
              "    width: Kirigami.Units.gridUnit * "
                  + QByteArray::number(units)
                  + "\n"
                    "    height: 752\n"
                    "    visible: true\n"
                    "    function rightEdge(item) { return item.mapToItem(pageItem, item.width, 0).x; }\n"
                    "    property real pageWidth: pageItem.width\n"
                    "    "
                  + page.toUtf8()
                  + " { id: pageItem; x: Kirigami.Units.gridUnit * 11; width: parent.width - x; height: parent.height }\n"
                    "}\n");

        QQmlApplicationEngine engine;
        PageStrings strings;
        engine.rootContext()->setContextObject(&strings);
        engine.addImportPath(dir.path());
        engine.load(QUrl::fromLocalFile(dir.filePath(QStringLiteral("main.qml"))));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));

        /* The password fields in the order the page declares them. */
        QList<QObject *> fields;
        const auto all = window->findChildren<QObject *>();
        for (QObject *o : all) {
            if (o->metaObject()->indexOfProperty("showPassword") >= 0 && o->metaObject()->indexOfProperty("echoMode") >= 0) {
                fields << o;
            }
        }
        QVERIFY(fields.size() >= 2 * pair + 2);
        QObject *first = fields.at(2 * pair);
        QObject *second = fields.at(2 * pair + 1);

        QVERIFY(QMetaObject::invokeMethod(first, "forceActiveFocus"));
        for (char c : {'a', 'b', 'c', 'd'}) {
            QTest::keyClick(window, c);
        }
        QTest::keyClick(window, Qt::Key_Tab);
        for (char c : {'a', 'b', 'c', 'd'}) {
            QTest::keyClick(window, c);
        }
        QCOMPARE(first->property("text").toString(), QStringLiteral("abcd"));
        QCOMPARE(second->property("text").toString(), QStringLiteral("abcd"));

        QObject *hint = nullptr;
        for (QObject *o : all) {
            if (o->metaObject()->indexOfProperty("shortAdvice") >= 0 && o->property("secret").toString() == QStringLiteral("abcd")
                && o->property("visible").toBool()) {
                hint = o;
            }
        }
        QVERIFY(hint);
        QCOMPARE(hint->property("text").toString(), hint->property("shortAdvice").toString());
        if (units == 58) {
            QTRY_COMPARE(hint->property("lineCount").toInt(), 1);
        } else {
            const qreal pageWidth = window->property("pageWidth").toReal();
            QVERIFY(pageWidth > 0);
            for (QObject *o : {hint, second}) {
                QVariant right;
                QVERIFY(QMetaObject::invokeMethod(window, "rightEdge", Q_RETURN_ARG(QVariant, right), Q_ARG(QVariant, QVariant::fromValue(o))));
                QVERIFY2(right.toReal() <= pageWidth + 0.5,
                         qPrintable(QStringLiteral("%1 ends at %2 on a page %3 wide")
                                        .arg(QLatin1String(o->metaObject()->className()))
                                        .arg(right.toReal())
                                        .arg(pageWidth)));
            }
        }

        QTest::keyClick(window, Qt::Key_Backtab, Qt::ShiftModifier);
        QObject *focus = window->property("activeFocusItem").value<QObject *>();
        QVERIFY(focus);
        QVERIFY2(focus != first, "Shift+Tab from the second field stopped in the first field, not on its reveal button");
        bool inside = false;
        for (QObject *up = focus; up && !inside; up = up->property("parent").value<QObject *>()) {
            inside = up == first;
        }
        QVERIFY2(inside, qPrintable(QStringLiteral("Shift+Tab from the second field went to %1").arg(QLatin1String(focus->metaObject()->className()))));

        QVERIFY(!first->property("showPassword").toBool());
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(first->property("showPassword").toBool());
        QCOMPARE(first->property("text").toString(), QStringLiteral("abcd"));
    }
};

QTEST_MAIN(InstallerTest)

#include "installertest.moc"
