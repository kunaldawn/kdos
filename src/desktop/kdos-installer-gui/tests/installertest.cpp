/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   installertest — the answer file, the event stream, the layout list, the
 *   live session's layout files, the password pairs' keyboard path, the
 *   application groups' keyboard path and names, the accent cards' radio
 *   group, the window's Quit and when it announces why Next is disabled
 *
 * The fixtures under fixtures/ are kinstall's own output: two streams a real
 * `kinstall --events json` wrote (refused answers; a dry run stopped for not
 * being root) and one success in the same shape. Beside them, a rules list,
 * an XKB symbols directory holding `us` and `de` only, a zone1970.tab, and
 * two mounts tables, the live medium's and an installed machine's.
 *
 * The Encryption, Account, Look and Applications pages, and the wizard window
 * with stand-ins for its pages, are loaded from the source tree's qml/, with
 * a Backend that holds what each reads and nothing else, under the style the
 * installer ships with, and driven with keys; the pages in a window of the
 * installer's default size.
 */

#include "answers.h"
#include "backend.h"
#include "eventstream.h"
#include "keyboardmodel.h"

#include <KConfig>
#include <KConfigGroup>
#include <KConfigWatcher>
#include <KFormat>
#include <KSharedConfig>

#include <QAccessible>
#include <QDBusConnection>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

using namespace Kdos;

/* The pages' i18n(), i18nc(), i18np() and i18ncp(), as KLocalizedContext
 * gives them: the text with its arguments filled in, untranslated, the
 * plural form chosen by the count alone. */
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
    Q_INVOKABLE QString i18np(const QString &one, const QString &many, int n, const QVariant &a = {}, const QVariant &b = {}) const
    {
        return fill(fill(n == 1 ? one : many, n, a), b, {});
    }
    Q_INVOKABLE QString i18ncp(const QString &, const QString &one, const QString &many, int n, const QVariant &a = {}, const QVariant &b = {},
                               const QVariant &c = {}) const
    {
        return fill(fill(n == 1 ? one : many, n, a), b, c);
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

    /* The wizard window, Main.qml from the source tree, with stand-ins for
     * its pages, each named by its objectName, and a Backend holding what
     * the window reads. */
    static void writeWizard(const QTemporaryDir &dir)
    {
        const QString mod = dir.filePath(QStringLiteral("org/kdos/installer"));
        if (!QDir().mkpath(mod)) {
            qFatal("cannot make %s", qPrintable(mod));
        }
        if (!QFile::copy(QStringLiteral(FIXTURES_DIR "/../../qml/Main.qml"), mod + QStringLiteral("/Main.qml"))) {
            qFatal("cannot copy Main.qml");
        }
        QByteArray qmldir = "module org.kdos.installer\nMain 1.0 Main.qml\nsingleton Backend 1.0 Backend.qml\n";
        for (const char *p : {"WelcomePage", "KeyboardPage", "DiskPage", "EncryptionPage", "AccountPage", "LookPage", "AppsPage", "SummaryPage",
                              "InstallPage", "DonePage"}) {
            const QByteArray name(p);
            write(mod + QLatin1Char('/') + QLatin1String(p) + QStringLiteral(".qml"),
                  "import QtQuick\n"
                  "import QtQuick.Controls as QQC2\n"
                  "Item {\n"
                  "    objectName: \""
                      + name + "\"\n"
                  "    property string heading\n"
                  "    property bool complete: true\n"
                  "    property string problem\n"
                  "    signal entered()\n"
                  "    signal retry()\n"
                      + QByteArray(name == "KeyboardPage" ? "    QQC2.TextField { objectName: \"search\" }\n" : "") + "}\n");
            qmldir += name + " 1.0 " + name + ".qml\n";
        }
        write(mod + QStringLiteral("/qmldir"), qmldir);
        write(mod + QStringLiteral("/Backend.qml"),
              "pragma Singleton\n"
              "import QtQuick\n"
              "QtObject {\n"
              "    signal installFinished(bool ok)\n"
              "    property var probe: ({ disks: [] })\n"
              "    property bool installing: false\n"
              "    property bool busy: false\n"
              "    property string error\n"
              "    property string logPath: \"/var/log/kinstall.log\"\n"
              "    property QtObject events: QtObject { property bool started: false }\n"
              "    property QtObject answers: QtObject {\n"
              "        property string plan: \"wipe\"\n"
              "        property string disk\n"
              "        property string root\n"
              "    }\n"
              "    function refreshProbe() {}\n"
              "    function refreshLook() {}\n"
              "    function refreshApps() {}\n"
              "    function formatSize(b) { return b + \" B\"; }\n"
              "    function diskName(p) { return p; }\n"
              "    function diskErased(p) { return \"\"; }\n"
              "}\n");
        write(dir.filePath(QStringLiteral("main.qml")), "import org.kdos.installer\nMain {}\n");
    }

    /* An item's Accessible attached object, where QML set one. */
    static QObject *accessible(QObject *item)
    {
        for (QObject *c : item->children()) {
            if (qstrcmp(c->metaObject()->className(), "QQuickAccessibleAttached") == 0) {
                return c;
            }
        }
        return nullptr;
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
    /* The pages run under the style the installer ships with, as main.cpp
     * sets it: the mnemonics and the focus a dialog opens on are the
     * style's, and the plain style has neither. */
    void initTestCase()
    {
        if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
            qputenv("QT_QUICK_CONTROLS_STYLE", "org.kde.desktop");
        }
    }

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

    /* The Keyboard page's choice as the live session types it: kxkbrc
     * lists the layout before us and keeps its other keys. */
    void liveLayout()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString cfg = home.path() + QStringLiteral("/config");
        QVERIFY(QDir().mkpath(cfg));
        write(cfg + QStringLiteral("/kxkbrc"), "[Layout]\nLayoutList=us\nVariantList=\nModel=pc105\n");

        QVERIFY(Backend::writeLiveLayout(cfg, QStringLiteral("de"), QStringLiteral("nodeadkeys")));

        KConfig kxkb(cfg + QStringLiteral("/kxkbrc"), KConfig::SimpleConfig);
        const KConfigGroup layout(&kxkb, QStringLiteral("Layout"));
        QCOMPARE(layout.readEntry("LayoutList", QString()), QStringLiteral("de,us"));
        QCOMPARE(layout.readEntry("VariantList", QString()), QStringLiteral("nodeadkeys,"));
        QCOMPARE(layout.readEntry("Use", false), true);
        QCOMPARE(layout.readEntry("Model", QString()), QStringLiteral("pc105"));

        /* us alone. */
        const QString bare = home.path() + QStringLiteral("/bare");
        QVERIFY(Backend::writeLiveLayout(bare, QStringLiteral("us"), QString()));
        KConfig us(bare + QStringLiteral("/kxkbrc"), KConfig::SimpleConfig);
        QCOMPARE(KConfigGroup(&us, QStringLiteral("Layout")).readEntry("LayoutList", QString()), QStringLiteral("us"));
        QCOMPARE(KConfigGroup(&us, QStringLiteral("Layout")).readEntry("VariantList", QString()), QString());

        /* A name outside XKB's set writes nothing. */
        const QString odd = home.path() + QStringLiteral("/odd");
        QVERIFY(!Backend::writeLiveLayout(odd, QStringLiteral("de\nUse=false"), QString()));
        QVERIFY(!Backend::writeLiveLayout(odd, QStringLiteral("de"), QStringLiteral("a b")));
        QVERIFY(!QFileInfo::exists(odd + QStringLiteral("/kxkbrc")));
    }

    /* The notice reaches a KConfigWatcher on kxkbrc, as KWin's keyboard
     * layout code holds one, naming the [Layout] keys it rewrote. A write
     * without it leaves KWin on the old keymap. */
    void liveLayoutNotice()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY2(bus.isConnected(), "the test runs under dbus-run-session");
        KConfigWatcher::Ptr watcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kxkbrc"), KConfig::NoGlobals));
        QSignalSpy spy(watcher.data(), &KConfigWatcher::configChanged);
        QVERIFY(bus.send(Backend::layoutNotice()));
        QVERIFY(spy.wait(5000));
        const KConfigGroup group = spy.first().at(0).value<KConfigGroup>();
        QCOMPARE(group.name(), QStringLiteral("Layout"));
        const QByteArrayList names = spy.first().at(1).value<QByteArrayList>();
        QVERIFY(names.contains("LayoutList"));
        QVERIFY(names.contains("VariantList"));
        QVERIFY(names.contains("Use"));
    }

    void zones()
    {
        const QStringList z = Backend::readZones(QStringLiteral(FIXTURES_DIR "/zone1970.tab"));
        QCOMPARE(z, (QStringList{QStringLiteral("UTC"), QStringLiteral("Africa/Nairobi"), QStringLiteral("America/New_York"),
                                 QStringLiteral("Asia/Kolkata"), QStringLiteral("Europe/Berlin")}));
        QCOMPARE(Backend::readZones(QStringLiteral(FIXTURES_DIR "/absent.tab")), QStringList{QStringLiteral("UTC")});
    }

    /* The release is the record's VERSION; a root with no record gives
     * os-release's VERSION_ID, with "+BUILD_ID" when it has one; a root
     * with neither gives "". */
    void release()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("etc"))));
        auto write = [&](const QString &name, const QByteArray &data) {
            QFile f(dir.filePath(QStringLiteral("etc/") + name));
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(data);
        };
        QCOMPARE(Backend::readRelease(dir.path()), QString());
        write(QStringLiteral("os-release"), "NAME=\"KDOS\"\nVERSION_ID=0.2.1\n");
        QCOMPARE(Backend::readRelease(dir.path()), QStringLiteral("0.2.1"));
        write(QStringLiteral("os-release"), "NAME=\"KDOS\"\nVERSION_ID=0.2.1\nBUILD_ID=\"047d4b99779b-dirty\"\n");
        QCOMPARE(Backend::readRelease(dir.path()), QStringLiteral("0.2.1+047d4b99779b-dirty"));
        write(QStringLiteral("kdos-release"), "# record\nVERSION=\"0.2.1\"\nBUILD_ID=\"047d4b99779b-dirty\"\n");
        QCOMPARE(Backend::readRelease(dir.path()), QStringLiteral("0.2.1"));
    }

    void live()
    {
        /* An overlay at `/` is the live medium; one under a container's
         * store is not, and an unreadable table is not either. */
        QVERIFY(Backend::liveRoot(QStringLiteral(FIXTURES_DIR "/mounts-live")));
        QVERIFY(!Backend::liveRoot(QStringLiteral(FIXTURES_DIR "/mounts-installed")));
        QVERIFY(!Backend::liveRoot(QStringLiteral(FIXTURES_DIR "/absent.mounts")));
    }

    /* Welcome stops when no disk can take the install, saying why in the
     * first refused disk's words, and goes on once one can. */
    void welcomeNeedsUsableDisk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mod = dir.filePath(QStringLiteral("org/kdos/installer"));
        QVERIFY(QDir().mkpath(mod));
        const QString src = QStringLiteral(FIXTURES_DIR "/../../qml/");
        for (const char *f : {"WizardPage.qml", "WelcomePage.qml"}) {
            QVERIFY(QFile::copy(src + QLatin1String(f), mod + QLatin1Char('/') + QLatin1String(f)));
        }
        write(mod + QStringLiteral("/qmldir"),
              "module org.kdos.installer\n"
              "WizardPage 1.0 WizardPage.qml\n"
              "WelcomePage 1.0 WelcomePage.qml\n"
              "singleton Backend 1.0 Backend.qml\n");
        write(mod + QStringLiteral("/Backend.qml"),
              "pragma Singleton\n"
              "import QtQuick\n"
              "QtObject {\n"
              "    property var probe: ({ disks: [{ path: \"/dev/sda\", fits: false, problem: \"/dev/sda is the medium this runs from\" }] })\n"
              "    property bool busy: false\n"
              "    property var timeZones: [\"UTC\"]\n"
              "    property QtObject answers: QtObject {\n"
              "        signal changed()\n"
              "        property string timezone: \"UTC\"\n"
              "    }\n"
              "    function localTime(z) { return \"12:00\"; }\n"
              "    function formatSize(b) { return b + \" B\"; }\n"
              "}\n");
        write(dir.filePath(QStringLiteral("main.qml")),
              "import QtQuick\n"
              "import QtQuick.Window\n"
              "import org.kdos.installer\n"
              "Window {\n"
              "    width: 900; height: 700; visible: true\n"
              "    property alias page: pageItem\n"
              "    function setProbe(p) { Backend.probe = p; }\n"
              "    WelcomePage { id: pageItem; anchors.fill: parent }\n"
              "}\n");

        QQmlApplicationEngine engine;
        PageStrings strings;
        engine.rootContext()->setContextObject(&strings);
        engine.addImportPath(dir.path());
        engine.load(QUrl::fromLocalFile(dir.filePath(QStringLiteral("main.qml"))));
        QCOMPARE(engine.rootObjects().size(), 1);
        QObject *page = engine.rootObjects().constFirst()->property("page").value<QObject *>();
        QVERIFY(page);
        QCOMPARE(page->property("complete").toBool(), false);
        QCOMPARE(page->property("problem").toString(), QStringLiteral("No disk here can take the install: /dev/sda is the medium this runs from."));

        QVERIFY(QMetaObject::invokeMethod(engine.rootObjects().constFirst(), "setProbe",
                                          Q_ARG(QVariant, QVariant(QVariantMap{{QStringLiteral("disks"),
                                          QVariantList{QVariantMap{{QStringLiteral("path"), QStringLiteral("/dev/sda")},
                                                                   {QStringLiteral("fits"), false},
                                                                   {QStringLiteral("problem"), QStringLiteral("/dev/sda is the medium this runs from")}},
                                                       QVariantMap{{QStringLiteral("path"), QStringLiteral("/dev/vda")}, {QStringLiteral("fits"), true}}}}}))));
        QCOMPARE(page->property("complete").toBool(), true);
        QCOMPARE(page->property("problem").toString(), QString());
    }

    /* A disk and what an erase destroys, in the words the Disk page's row,
     * the Summary's banner and the prompt share: a two-slot install's disk
     * names its ESP by label and its physical volume partition by group,
     * never as blkid's LVM2_member; a whole-disk PV names its group, a
     * written stick its ISO 9660 image, and an empty disk lists nothing to
     * destroy. */
    void diskWords()
    {
        const auto part = [](const char *fstype, const char *label, double sectors, bool esp, bool biosBoot, const char *vg) {
            return QVariantMap{{QStringLiteral("fstype"), QString::fromUtf8(fstype)},
                               {QStringLiteral("label"), QString::fromUtf8(label)},
                               {QStringLiteral("sectors"), sectors},
                               {QStringLiteral("esp"), esp},
                               {QStringLiteral("bios_boot"), biosBoot},
                               {QStringLiteral("vg"), QString::fromUtf8(vg)}};
        };
        const auto size = [](double sectors) {
            return KFormat().formatByteSize(sectors * 512, 1);
        };
        const QVariantMap reinstall{
            {QStringLiteral("path"), QStringLiteral("/dev/vda")},
            {QStringLiteral("model"), QString()},
            {QStringLiteral("bytes"), 30.0 * 1073741824},
            {QStringLiteral("fstype"), QString()},
            {QStringLiteral("partitions"),
             QVariantList{part("vfat", "KDOS_EFI", 1048576, true, false, ""), part("", "", 2048, false, true, ""),
                          part("LVM2_member", "", 61861854, false, false, "kdos")}}};
        const QString contents = QStringLiteral("EFI system “KDOS_EFI” %1, BIOS boot %2, LVM physical volume (volume group kdos) %3")
                                     .arg(size(1048576), size(2048), size(61861854));
        QCOMPARE(Backend::diskContents(reinstall), contents);
        QCOMPARE(Backend::diskHolds(reinstall), QStringLiteral("Holds: ") + contents);
        QVERIFY(!Backend::diskHolds(reinstall).contains(QStringLiteral("LVM2_member")));
        QCOMPARE(Backend::diskTitle(reinstall), QStringLiteral("Disk, %1 (/dev/vda)").arg(KFormat().formatByteSize(30.0 * 1073741824, 1)));

        const QVariantMap windows{{QStringLiteral("path"), QStringLiteral("/dev/nvme0n1")},
                                  {QStringLiteral("model"), QStringLiteral("QEMU NVMe Ctrl")},
                                  {QStringLiteral("sectors"), 62914560.0},
                                  {QStringLiteral("partitions"),
                                   QVariantList{part("ntfs", "Windows", 37748736, false, false, ""),
                                                part("ext4", "linuxhome", 23068672, false, false, "")}}};
        QCOMPARE(Backend::diskContents(windows),
                 QStringLiteral("ntfs “Windows” %1, ext4 “linuxhome” %2").arg(size(37748736), size(23068672)));
        QCOMPARE(Backend::diskTitle(windows), QStringLiteral("QEMU NVMe Ctrl, %1 (/dev/nvme0n1)").arg(size(62914560)));

        const QVariantMap pv{{QStringLiteral("path"), QStringLiteral("/dev/sdb")},
                             {QStringLiteral("fstype"), QStringLiteral("LVM2_member")},
                             {QStringLiteral("vg"), QStringLiteral("kdos")},
                             {QStringLiteral("partitions"), QVariantList{}}};
        QCOMPARE(Backend::diskContents(pv), QStringLiteral("LVM physical volume (volume group kdos)"));
        QCOMPARE(Backend::diskHolds(pv), QStringLiteral("LVM physical volume (volume group kdos)"));

        /* A written stick: the image on the whole device, never its
         * hybrid partitions as empty. */
        const double image = 3463680.0 * 512;
        const QVariantMap stick{{QStringLiteral("path"), QStringLiteral("/dev/sda")},
                                {QStringLiteral("fstype"), QStringLiteral("iso9660")},
                                {QStringLiteral("label"), QStringLiteral("KDOS_LIVE")},
                                {QStringLiteral("fs_bytes"), image},
                                {QStringLiteral("iso_image"), true},
                                {QStringLiteral("partitions"),
                                 QVariantList{part("", "", 64, false, false, ""), part("vfat", "", 4096, true, false, ""),
                                              part("", "", 3463680, false, false, "")}}};
        QCOMPARE(Backend::diskHolds(stick),
                 QStringLiteral("Holds: ISO 9660 image “KDOS_LIVE” on the whole disk, %1").arg(KFormat().formatByteSize(image, 1)));
        QVERIFY(!Backend::diskHolds(stick).contains(QStringLiteral("empty")));
        QVariantMap unlabelled = stick;
        unlabelled.insert(QStringLiteral("label"), QString());
        unlabelled.insert(QStringLiteral("fs_bytes"), 0.0);
        unlabelled.insert(QStringLiteral("partitions"), QVariantList{});
        QCOMPARE(Backend::diskContents(unlabelled), QStringLiteral("ISO 9660 image on the whole disk"));
        /* Repartitioned over the image: the descriptor still names the
         * whole device, but the partitions are what an erase destroys. */
        QVariantMap repartitioned = stick;
        repartitioned.insert(QStringLiteral("iso_image"), false);
        repartitioned.insert(QStringLiteral("partitions"), QVariantList{part("ext4", "data", 62912512, false, false, "")});
        QCOMPARE(Backend::diskHolds(repartitioned), QStringLiteral("Holds: ext4 “data” %1").arg(size(62912512)));

        /* A Ventoy stick: the boot partition's node is remapped to the
         * ISO, so the partition is named as Ventoy's, never by what the
         * node reads; another mapping by its dm name. */
        QVariantMap vtoyefi = part("vfat", "VTOYEFI", 65536, true, false, "");
        vtoyefi.insert(QStringLiteral("remap"), QStringLiteral("ventoy"));
        const QVariantMap ventoy{{QStringLiteral("path"), QStringLiteral("/dev/sda")},
                                 {QStringLiteral("partitions"), QVariantList{part("exfat", "Ventoy", 16777216, false, false, ""), vtoyefi}}};
        QCOMPARE(Backend::diskHolds(ventoy),
                 QStringLiteral("Holds: exfat “Ventoy” %1, Ventoy's boot partition %2").arg(size(16777216), size(65536)));
        vtoyefi.insert(QStringLiteral("remap"), QStringLiteral("isodata"));
        QCOMPARE(Backend::partitionText(vtoyefi), QStringLiteral("mapped to isodata %1").arg(size(65536)));

        const QVariantMap empty{{QStringLiteral("path"), QStringLiteral("/dev/sdc")}, {QStringLiteral("partitions"), QVariantList{}}};
        QCOMPARE(Backend::diskContents(empty), QString());
        QCOMPARE(Backend::diskHolds(empty), QStringLiteral("Empty"));

        /* The reuse plan names each partition it formats by node and
         * contents; a logical volume, in no partition list, by its path. */
        QVariantMap esp = part("vfat", "KDOS_EFI", 1048576, true, false, "");
        esp.insert(QStringLiteral("path"), QStringLiteral("/dev/vda1"));
        QVariantMap fat = part("vfat", "PHOTOS", 1048576, false, false, "");
        fat.insert(QStringLiteral("path"), QStringLiteral("/dev/sdb1"));
        const QVariantMap probe{{QStringLiteral("disks"),
                                 QVariantList{QVariantMap{{QStringLiteral("path"), QStringLiteral("/dev/vda")},
                                                          {QStringLiteral("partitions"), QVariantList{esp}}}}}};
        QCOMPARE(Backend::partitionNamed(probe, QStringLiteral("/dev/vda1")),
                 QStringLiteral("/dev/vda1 (EFI system “KDOS_EFI” %1)").arg(size(1048576)));
        QCOMPARE(Backend::partitionNamed(probe, QStringLiteral("/dev/mapper/kdos-root_a")), QStringLiteral("/dev/mapper/kdos-root_a"));

        /* Only a lone partition typed as an ESP is chosen unasked: a lone
         * FAT data partition would be formatted by default. */
        QCOMPARE(Backend::soleEsp(QVariantList{esp}), QStringLiteral("/dev/vda1"));
        QCOMPARE(Backend::soleEsp(QVariantList{fat}), QString());
        QCOMPARE(Backend::soleEsp(QVariantList{esp, fat}), QString());
        QCOMPARE(Backend::soleEsp(QVariantList{}), QString());
    }

    /* The install size is what the copy writes, the total the copy line
     * counts to, split when there is a box store of 1 MiB or more (below
     * that it is the store's empty directories); a probe handed root's
     * payload takes root's figures. */
    void installSize()
    {
        const auto size = [](double b) { return KFormat().formatByteSize(b, 1); };
        const double sys = 4.4 * 1073741824, store = 0.7 * 1073741824;
        QCOMPARE(Backend::installSize({{QStringLiteral("copy_system_bytes"), sys}, {QStringLiteral("copy_store_bytes"), 0.0}}), size(sys));
        QCOMPARE(Backend::installSize({{QStringLiteral("copy_system_bytes"), sys}, {QStringLiteral("copy_store_bytes"), store}}),
                 QStringLiteral("%1 — %2 system and %3 boxed-app store").arg(size(sys + store), size(sys), size(store)));
        const double empty = 120 * 1024.0, big = 1.9 * 1073741824;
        QCOMPARE(Backend::installSize({{QStringLiteral("copy_system_bytes"), sys}, {QStringLiteral("copy_store_bytes"), empty}}), size(sys + empty));
        QCOMPARE(Backend::installSize({{QStringLiteral("copy_system_bytes"), sys}, {QStringLiteral("copy_store_bytes"), big}}),
                 QStringLiteral("%1 — %2 system and %3 boxed-app store").arg(size(sys + big), size(sys), size(big)));
        QCOMPARE(Backend::installSize({{QStringLiteral("payload_kb"), 5345280.0}}), QStringLiteral("—"));

        const QVariantMap root{{QStringLiteral("copy_system_bytes"), sys}, {QStringLiteral("copy_store_bytes"), store}};
        const QVariantMap given{{QStringLiteral("payload_kb"), 5345280.0},
                                {QStringLiteral("copy_system_bytes"), QVariant()},
                                {QStringLiteral("copy_store_bytes"), QVariant()}};
        QCOMPARE(Backend::withCopyBytes(given, root).value(QStringLiteral("copy_system_bytes")).toDouble(), sys);
        QCOMPARE(Backend::withCopyBytes(given, root).value(QStringLiteral("copy_store_bytes")).toDouble(), store);
        const QVariantMap walked{{QStringLiteral("copy_system_bytes"), 1024.0}, {QStringLiteral("copy_store_bytes"), 0.0}};
        QCOMPARE(Backend::withCopyBytes(walked, root), walked);
    }

    /* The Summary's swap row names the kind the plan makes. */
    void swapText()
    {
        const QVariantMap file{{QStringLiteral("swap"), QStringLiteral("file")}, {QStringLiteral("swap_mb"), 4096}, {QStringLiteral("lvm"), QVariant()}};
        QCOMPARE(Backend::swapText(file), QStringLiteral("4096 MiB, a file in the root"));
        QVariantMap part = file;
        part.insert(QStringLiteral("swap"), QStringLiteral("partition"));
        QCOMPARE(Backend::swapText(part), QStringLiteral("4096 MiB, a partition"));
        /* One slot on LVM: the swap is a file in the root. */
        QVariantMap oneSlot = file;
        oneSlot.insert(QStringLiteral("lvm"), QVariantMap{{QStringLiteral("ab"), false}, {QStringLiteral("swap_mb"), 0}});
        QCOMPARE(Backend::swapText(oneSlot), QStringLiteral("4096 MiB, a file in the root"));
        /* Two slots make it a volume, whichever kind was asked for. */
        QVariantMap ab = part;
        ab.insert(QStringLiteral("lvm"), QVariantMap{{QStringLiteral("ab"), true}, {QStringLiteral("swap_mb"), 4096}});
        QCOMPARE(Backend::swapText(ab), QStringLiteral("4096 MiB, a volume in the group"));
        const QVariantMap none{{QStringLiteral("swap"), QStringLiteral("none")}, {QStringLiteral("swap_mb"), 0}};
        QCOMPARE(Backend::swapText(none), QStringLiteral("None"));
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

    /* Type, Tab, type, Shift+Tab, Space: while only the first field holds
     * text the page asks for the second, and neither the first's strength
     * line nor the second field moves; what is typed after the Tab lands
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
                    "    function topOf(item) { return item.mapToItem(null, 0, 0).y; }\n"
                    "    property real pageWidth: pageItem.width\n"
                    "    property string problem: pageItem.problem\n"
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
        /* On the account's pair, the name and root's pair are answered, so
         * the reason shown is the pair's own. */
        if (page == QStringLiteral("AccountPage") && pair == 0) {
            QObject *name = window->findChild<QObject *>(QStringLiteral("fullname"));
            QVERIFY(name && fields.size() >= 4);
            name->setProperty("text", QStringLiteral("Ann"));
            fields.at(2)->setProperty("text", QStringLiteral("rootroot"));
            fields.at(3)->setProperty("text", QStringLiteral("rootroot"));
        }

        /* The first field's strength line, and where the second field sits
         * with nothing typed. */
        QObject *hint = nullptr;
        for (QObject *o : all) {
            if (o->metaObject()->indexOfProperty("shortAdvice") >= 0 && o->property("field").value<QObject *>() == first) {
                hint = o;
            }
        }
        QVERIFY(hint);
        QVERIFY(hint->property("visible").toBool());
        const auto topOf = [window](QObject *o) {
            QVariant y;
            if (!QMetaObject::invokeMethod(window, "topOf", Q_RETURN_ARG(QVariant, y), Q_ARG(QVariant, QVariant::fromValue(o)))) {
                qFatal("topOf failed");
            }
            return y.toReal();
        };
        QTest::qWait(100);
        const qreal emptyHeight = hint->property("height").toReal();
        const qreal emptyTop = topOf(second);
        QVERIFY(emptyHeight > 0);

        QVERIFY(QMetaObject::invokeMethod(first, "forceActiveFocus"));
        for (char c : {'a', 'b', 'c', 'd'}) {
            QTest::keyClick(window, c);
        }
        /* The first field typed in, the second empty: the page asks for the
         * second, never a mismatch, and nothing below the first moved. */
        QCOMPARE(window->property("problem").toString(),
                 page == QStringLiteral("EncryptionPage") ? QStringLiteral("Type the passphrase again.")
                 : pair == 1                              ? QStringLiteral("Type root's password again.")
                                                          : QStringLiteral("Type the password again."));
        QTest::qWait(100);
        QCOMPARE(hint->property("height").toReal(), emptyHeight);
        QCOMPARE(topOf(second), emptyTop);
        QTest::keyClick(window, Qt::Key_Tab);
        for (char c : {'a', 'b', 'c', 'd'}) {
            QTest::keyClick(window, c);
        }
        QCOMPARE(first->property("text").toString(), QStringLiteral("abcd"));
        QCOMPARE(second->property("text").toString(), QStringLiteral("abcd"));

        QCOMPARE(hint->property("secret").toString(), QStringLiteral("abcd"));
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

    /* The Keyboard page's search: Alt with a letter types nothing into
     * it, and a search that matches no layout says so where the list was. */
    void keyboardSearch()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mod = dir.filePath(QStringLiteral("org/kdos/installer"));
        QVERIFY(QDir().mkpath(mod));
        const QString src = QStringLiteral(FIXTURES_DIR "/../../qml/");
        for (const char *f : {"WizardPage.qml", "KeyboardPage.qml"}) {
            QVERIFY(QFile::copy(src + QLatin1String(f), mod + QLatin1Char('/') + QLatin1String(f)));
        }
        write(mod + QStringLiteral("/qmldir"),
              "module org.kdos.installer\n"
              "WizardPage 1.0 WizardPage.qml\n"
              "KeyboardPage 1.0 KeyboardPage.qml\n"
              "singleton Backend 1.0 Backend.qml\n");
        write(mod + QStringLiteral("/Backend.qml"),
              "pragma Singleton\n"
              "import QtQuick\n"
              "QtObject {\n"
              "    property var keyboards: testKeyboards\n"
              "    property QtObject answers: QtObject {\n"
              "        property string xkbLayout: \"us\"\n"
              "        property string xkbVariant\n"
              "    }\n"
              "    function applyLiveLayout(l, v) {}\n"
              "}\n");
        write(dir.filePath(QStringLiteral("main.qml")),
              "import QtQuick\n"
              "import QtQuick.Window\n"
              "import org.kdos.installer\n"
              "Window {\n"
              "    width: 800\n"
              "    height: 752\n"
              "    visible: true\n"
              "    KeyboardPage { anchors.fill: parent }\n"
              "}\n");

        KeyboardModel keyboards;
        keyboards.parse(fixture("evdev.lst"));
        QQmlApplicationEngine engine;
        PageStrings strings;
        engine.rootContext()->setContextObject(&strings);
        engine.rootContext()->setContextProperty(QStringLiteral("testKeyboards"), &keyboards);
        engine.addImportPath(dir.path());
        engine.load(QUrl::fromLocalFile(dir.filePath(QStringLiteral("main.qml"))));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));

        QObject *search = nullptr;
        for (QObject *o : window->findChildren<QObject *>()) {
            if (o->property("placeholderText").toString() == QStringLiteral("Search layouts…")) {
                search = o;
            }
        }
        QObject *list = window->findChild<QObject *>(QStringLiteral("layouts"));
        QObject *noMatch = window->findChild<QObject *>(QStringLiteral("noMatch"));
        QVERIFY(search && list && noMatch);
        QTRY_COMPARE(list->property("count").toInt(), 3);
        QVERIFY(!noMatch->property("visible").toBool());

        QVERIFY(QMetaObject::invokeMethod(search, "forceActiveFocus"));
        for (char c : {'g', 'e', 'r'}) {
            QTest::keyClick(window, c);
        }
        QTRY_COMPARE(list->property("count").toInt(), 1);
        QTest::keyClick(window, 'q', Qt::AltModifier);
        QTest::qWait(100);
        QCOMPARE(search->property("text").toString(), QStringLiteral("ger"));
        QCOMPARE(list->property("count").toInt(), 1);
        QVERIFY(!noMatch->property("visible").toBool());
        QTest::keyClick(window, 'Q', Qt::ShiftModifier);
        QCOMPARE(search->property("text").toString(), QStringLiteral("gerQ"));
        QTest::keyClick(window, Qt::Key_Backspace);
        QTest::keyClick(window, 'q');
        QCOMPARE(search->property("text").toString(), QStringLiteral("gerq"));
        QTRY_COMPARE(list->property("count").toInt(), 0);
        QTRY_VERIFY(noMatch->property("visible").toBool());
        QCOMPARE(noMatch->property("text").toString(), QStringLiteral("No layout matches “gerq”"));

        search->setProperty("text", QString());
        QTRY_COMPARE(list->property("count").toInt(), 3);
        QTRY_VERIFY(!noMatch->property("visible").toBool());
    }

    /* Tab reaches every group row of the Applications page, native and
     * boxed, framed in the focus colour while it holds the keyboard, and
     * Space there ticks the group. */
    void appsByKeyboard()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mod = dir.filePath(QStringLiteral("org/kdos/installer"));
        QVERIFY(QDir().mkpath(mod));
        const QString src = QStringLiteral(FIXTURES_DIR "/../../qml/");
        for (const char *f : {"WizardPage.qml", "AppsPage.qml"}) {
            QVERIFY(QFile::copy(src + QLatin1String(f), mod + QLatin1Char('/') + QLatin1String(f)));
        }
        write(mod + QStringLiteral("/qmldir"),
              "module org.kdos.installer\n"
              "WizardPage 1.0 WizardPage.qml\n"
              "AppsPage 1.0 AppsPage.qml\n"
              "singleton Backend 1.0 Backend.qml\n");
        write(mod + QStringLiteral("/Backend.qml"),
              "pragma Singleton\n"
              "import QtQuick\n"
              "QtObject {\n"
              "    id: backend\n"
              "    property var plan: ({ binhost: true })\n"
              "    property var apps: ({ route: \"\",\n"
              "        native: [{ id: \"games\", title: \"Games\", ports: 3 }, { id: \"office\", title: \"Office\", ports: 1 }],\n"
              "        groups: [{ id: \"science\", desc: \"Science\", apps: 1, bytes: 100 }] })\n"
              "    property string ticked\n"
              "    property QtObject answers: QtObject {\n"
              "        property var groups: []\n"
              "        property var apps: []\n"
              "        property bool alienApps: false\n"
              "        function setGroupChosen(id, on) { if (on) backend.ticked += \"native:\" + id + \" \"; }\n"
              "        function setAppChosen(id, on) { if (on) backend.ticked += \"boxed:\" + id + \" \"; }\n"
              "    }\n"
              "    function refreshApps() {}\n"
              "    function checkPlan() {}\n"
              "    function formatSize(b) { return b + \" B\"; }\n"
              "}\n");
        write(dir.filePath(QStringLiteral("main.qml")),
              "import QtQuick\n"
              "import QtQuick.Window\n"
              "import org.kde.kirigami as Kirigami\n"
              "import org.kdos.installer\n"
              "Window {\n"
              "    width: Kirigami.Units.gridUnit * 58\n"
              "    height: 752\n"
              "    visible: true\n"
              "    property string ticked: Backend.ticked\n"
              "    AppsPage { x: Kirigami.Units.gridUnit * 11; width: parent.width - x; height: parent.height }\n"
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

        /* The group rows: the page's check delegates, whose text is a group's. */
        const QStringList names{QStringLiteral("Games"), QStringLiteral("Office"), QStringLiteral("Science")};
        QSet<QString> reached;
        for (int i = 0; i < 12 && reached.size() < names.size(); ++i) {
            QTest::keyClick(window, Qt::Key_Tab);
            QObject *focus = window->property("activeFocusItem").value<QObject *>();
            QVERIFY(focus);
            const QString text = focus->property("text").toString();
            if (!names.contains(text) || reached.contains(text)) {
                continue;
            }
            reached.insert(text);
            QVERIFY(focus->property("visualFocus").toBool());
            bool framed = false;
            for (QObject *c : focus->children()) {
                QObject *border = c->property("border").value<QObject *>();
                framed |= border && border->property("width").toInt() == 2 && c->property("visible").toBool();
            }
            QVERIFY2(framed, qPrintable(QStringLiteral("%1 has the keyboard and no focus frame").arg(text)));
            if (text == QStringLiteral("Science")) {
                QObject *a = accessible(focus);
                QVERIFY(a);
                QCOMPARE(a->property("name").toString(), QStringLiteral("Science, 1 application, about 100 B"));
            }
            QTest::keyClick(window, Qt::Key_Space);
            QVERIFY(focus->property("checked").toBool());
        }
        QCOMPARE(reached.size(), names.size());
        QCOMPARE(window->property("ticked").toString(), QStringLiteral("native:games native:office boxed:science "));
    }

    /* The Look page's accent cards: one radio group under one Tab stop, the
     * chosen card; the arrow keys move between the cards and leave the
     * answer alone, Space and Enter choose the focused one, and the next
     * Tab leaves the group. */
    void lookByKeyboard()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mod = dir.filePath(QStringLiteral("org/kdos/installer"));
        QVERIFY(QDir().mkpath(mod));
        const QString src = QStringLiteral(FIXTURES_DIR "/../../qml/");
        for (const char *f : {"WizardPage.qml", "LookPage.qml"}) {
            QVERIFY(QFile::copy(src + QLatin1String(f), mod + QLatin1Char('/') + QLatin1String(f)));
        }
        write(mod + QStringLiteral("/qmldir"),
              "module org.kdos.installer\n"
              "WizardPage 1.0 WizardPage.qml\n"
              "LookPage 1.0 LookPage.qml\n"
              "singleton Backend 1.0 Backend.qml\n");
        write(mod + QStringLiteral("/Backend.qml"),
              "pragma Singleton\n"
              "import QtQuick\n"
              "QtObject {\n"
              "    property var look: ({ accents: [{ name: \"bone\", lookandfeel: \"\", default: true },\n"
              "        { name: \"phosphor\", lookandfeel: \"\" }, { name: \"amber\", lookandfeel: \"\" }] })\n"
              "    property QtObject answers: QtObject {\n"
              "        property string theme: \"bone\"\n"
              "        property string desk: \"kdos\"\n"
              "    }\n"
              "    function refreshLook() {}\n"
              "    function accentColors(n) { return ({}); }\n"
              "    function accentPreview(l) { return \"\"; }\n"
              "    function accentLabel(n) { return n.charAt(0).toUpperCase() + n.slice(1); }\n"
              "    function deskTitle(n) { return n; }\n"
              "}\n");
        write(dir.filePath(QStringLiteral("main.qml")),
              "import QtQuick\n"
              "import QtQuick.Window\n"
              "import org.kde.kirigami as Kirigami\n"
              "import org.kdos.installer\n"
              "Window {\n"
              "    width: Kirigami.Units.gridUnit * 58\n"
              "    height: 752\n"
              "    visible: true\n"
              "    property string theme: Backend.answers.theme\n"
              "    LookPage { x: Kirigami.Units.gridUnit * 11; width: parent.width - x; height: parent.height }\n"
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

        const auto focused = [window]() {
            return window->property("activeFocusItem").value<QObject *>();
        };
        const auto name = [](QObject *item) {
            QObject *a = item ? accessible(item) : nullptr;
            return a ? a->property("name").toString() : QString();
        };
        const auto isCard = [](QObject *item) {
            QObject *a = item ? accessible(item) : nullptr;
            return a && a->property("role").toInt() == QAccessible::RadioButton;
        };

        /* Every Tab stop before the desk rows: exactly one is a card. */
        QStringList cards;
        QObject *card = nullptr;
        for (int i = 0; i < 8; ++i) {
            QTest::keyClick(window, Qt::Key_Tab);
            QObject *f = focused();
            QVERIFY(f);
            if (f->property("text").toString().startsWith(QStringLiteral("kdos"))) {
                break;
            }
            if (isCard(f)) {
                QVERIFY(f->property("visualFocus").toBool());
                cards << name(f);
                card = card ? card : f;
            }
        }
        QCOMPARE(cards, QStringList{QStringLiteral("Bone accent")});
        QVERIFY(card->property("checked").toBool());
        QObject *grid = card->property("parent").value<QObject *>();
        QVERIFY(grid && accessible(grid));
        QCOMPARE(accessible(grid)->property("role").toInt(), int(QAccessible::Grouping));
        QCOMPARE(name(grid), QStringLiteral("Accents"));

        /* Back onto the group from the desk rows, then across it. */
        QTest::keyClick(window, Qt::Key_Backtab, Qt::ShiftModifier);
        QCOMPARE(name(focused()), QStringLiteral("Bone accent"));
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(name(focused()), QStringLiteral("Phosphor accent"));
        QVERIFY(!focused()->property("checked").toBool());
        QCOMPARE(window->property("theme").toString(), QStringLiteral("bone"));
        QTest::keyClick(window, Qt::Key_Space);
        QCOMPARE(window->property("theme").toString(), QStringLiteral("phosphor"));
        QVERIFY(focused()->property("checked").toBool());
        QVERIFY(!card->property("checked").toBool());

        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(name(focused()), QStringLiteral("Amber accent"));
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(window->property("theme").toString(), QStringLiteral("amber"));
        QVERIFY(focused()->property("checked").toBool());

        /* Enter on a card a click unchecked before: it is checked again. */
        QTest::keyClick(window, Qt::Key_Left);
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(window->property("theme").toString(), QStringLiteral("phosphor"));
        QVERIFY(focused()->property("checked").toBool());
        QTest::keyClick(window, Qt::Key_Right);
        QVERIFY(!focused()->property("checked").toBool());

        /* The next Tab leaves the group, from the chosen card and from one
         * the arrows reached, Right from the last card wrapping to the
         * first, which lies before the chosen one. */
        QTest::keyClick(window, Qt::Key_Left);
        QCOMPARE(name(focused()), QStringLiteral("Phosphor accent"));
        QTest::keyClick(window, Qt::Key_Tab);
        QVERIFY(!isCard(focused()));
        QTest::keyClick(window, Qt::Key_Backtab, Qt::ShiftModifier);
        QCOMPARE(name(focused()), QStringLiteral("Phosphor accent"));
        QTest::keyClick(window, Qt::Key_Right);
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(name(focused()), QStringLiteral("Bone accent"));
        QTest::keyClick(window, Qt::Key_Tab);
        QVERIFY(!isCard(focused()));
        QTest::keyClick(window, Qt::Key_Backtab, Qt::ShiftModifier);
        QCOMPARE(name(focused()), QStringLiteral("Phosphor accent"));
        QTest::keyClick(window, Qt::Key_Left);
        QTest::keyClick(window, Qt::Key_Backtab, Qt::ShiftModifier);
        QVERIFY(!isCard(focused()));
    }

    /* The wizard window, with stand-in pages: Alt+Q in a text field leaves
     * the window and its answers alone; closing it past Welcome asks first,
     * the prompt focused on Keep Going, and Tab then Space on Quit closes
     * it; on Welcome it closes at once; and during an install the prompt
     * says the window stays open. */
    void quitAsks()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeWizard(dir);

        QQmlApplicationEngine engine;
        PageStrings strings;
        engine.rootContext()->setContextObject(&strings);
        engine.addImportPath(dir.path());
        engine.load(QUrl::fromLocalFile(dir.filePath(QStringLiteral("main.qml"))));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        const auto show = [window] {
            window->show();
            window->requestActivate();
            return QTest::qWaitForWindowActive(window);
        };
        QVERIFY(show());
        QObject *leave = window->findChild<QObject *>(QStringLiteral("leaveDialog"));
        QObject *quit = window->findChild<QObject *>(QStringLiteral("quitDialog"));
        QObject *search = window->findChild<QObject *>(QStringLiteral("search"));
        QVERIFY(leave && quit && search);
        const auto go = [window](int index) {
            return QMetaObject::invokeMethod(window, "go", Q_ARG(QVariant, index));
        };

        /* Welcome: nothing to lose, so the window closes at once. */
        window->close();
        QTRY_VERIFY(!window->isVisible());
        QVERIFY(!leave->property("opened").toBool());
        QVERIFY(show());

        /* Keyboard, typing in the search field: Alt+Q does nothing. */
        QVERIFY(go(1));
        QVERIFY(QMetaObject::invokeMethod(search, "forceActiveFocus"));
        QTest::keyClick(window, Qt::Key_Q, Qt::AltModifier);
        QTest::qWait(200);
        QVERIFY2(window->isVisible(), "Alt+Q closed the installer");
        QVERIFY(!leave->property("visible").toBool());

        /* Disk: closing asks; Return does not close the window. */
        QVERIFY(go(2));
        window->close();
        QTRY_VERIFY(leave->property("opened").toBool());
        QVERIFY(window->isVisible());
        QCOMPARE(leave->property("title").toString(), QStringLiteral("Leave the installer?"));
        QObject *focus = window->property("activeFocusItem").value<QObject *>();
        QVERIFY(focus);
        QCOMPARE(focus->property("text").toString(), QStringLiteral("Keep Going"));
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(200);
        QVERIFY2(window->isVisible(), "Return in the leave prompt closed the installer");
        QVERIFY(QMetaObject::invokeMethod(leave, "close"));
        QTRY_VERIFY(!leave->property("visible").toBool());

        /* Closing again, then Tab to Quit and Space: the window goes. */
        window->close();
        QTRY_VERIFY(leave->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_Tab);
        focus = window->property("activeFocusItem").value<QObject *>();
        QVERIFY(focus);
        QCOMPARE(focus->property("text").toString(), QStringLiteral("Quit"));
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!window->isVisible());
        QVERIFY(window->setProperty("closeAnyway", false));

        /* Back on Welcome after reaching Disk: the answers are still held,
         * so closing asks. */
        QVERIFY(show());
        QVERIFY(go(0));
        window->close();
        QTRY_VERIFY2(leave->property("opened").toBool(), "Welcome after Disk closed without asking");
        QVERIFY(window->isVisible());
        QVERIFY(QMetaObject::invokeMethod(leave, "close"));
        QTRY_VERIFY(!leave->property("visible").toBool());

        /* During an install that has started: the window stays. */
        QObject *backend = engine.singletonInstance<QObject *>("org.kdos.installer", "Backend");
        QVERIFY(backend);
        backend->setProperty("installing", true);
        backend->property("events").value<QObject *>()->setProperty("started", true);
        QVERIFY(go(8));
        window->close();
        QTRY_VERIFY(quit->property("opened").toBool());
        QVERIFY(window->isVisible());
        QVERIFY(!leave->property("visible").toBool());
        QCOMPARE(quit->property("subtitle").toString(),
                 QStringLiteral("The window stays open until the install ends: the disk is still being written, and its log is /var/log/kinstall.log."));
    }

    /* The reason beside Quit is announced only once settled: not while the
     * Backend is busy, so a probe's passing text is never spoken and the
     * reason it ends on is, within 800 ms of the probe ending; never within
     * 1500 ms of the last announcement, where Orca would drop it; and not
     * twice in a row on one page. */
    void problemAnnounced()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeWizard(dir);

        QQmlApplicationEngine engine;
        PageStrings strings;
        engine.rootContext()->setContextObject(&strings);
        engine.addImportPath(dir.path());
        engine.load(QUrl::fromLocalFile(dir.filePath(QStringLiteral("main.qml"))));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        window->show();
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        QObject *backend = engine.singletonInstance<QObject *>("org.kdos.installer", "Backend");
        QObject *welcome = window->findChild<QObject *>(QStringLiteral("WelcomePage"));
        QObject *problem = window->findChild<QObject *>(QStringLiteral("problem"));
        QVERIFY(backend && welcome && problem);
        const auto spoken = [problem] {
            return problem->property("spokenText").toString();
        };

        const QString looking = QStringLiteral("Looking at this computer's disks…");
        const QString nodisk = QStringLiteral("No disk was found to install on.");
        backend->setProperty("busy", true);
        welcome->setProperty("problem", looking);
        welcome->setProperty("complete", false);
        QTest::qWait(1200);
        QCOMPARE(problem->property("text").toString(), looking);
        QCOMPARE(spoken(), QString());

        QElapsedTimer clock;
        clock.start();
        welcome->setProperty("problem", nodisk);
        QTest::qWait(50);
        backend->setProperty("busy", false);
        QTRY_COMPARE_WITH_TIMEOUT(spoken(), nodisk, 2000);
        QVERIFY(clock.elapsed() >= 800);
        const qreal first = problem->property("lastSpoken").toReal();

        /* A second reason soon after waits out the gap. */
        clock.restart();
        welcome->setProperty("problem", QStringLiteral("Other reason."));
        QTRY_COMPARE_WITH_TIMEOUT(spoken(), QStringLiteral("Other reason."), 3000);
        QVERIFY2(problem->property("lastSpoken").toReal() - first >= 1500, "two announcements within 1500 ms");

        /* Away and back to the reason just spoken: not spoken again. */
        const qreal second = problem->property("lastSpoken").toReal();
        welcome->setProperty("problem", QStringLiteral("Passing."));
        QTest::qWait(100);
        welcome->setProperty("problem", QStringLiteral("Other reason."));
        QTest::qWait(2500);
        QCOMPARE(problem->property("lastSpoken").toReal(), second);
    }

    /* A reason that holds while the focused field is typed in is announced
     * only once typing pauses: each edit restarts the 800 ms settle. */
    void problemWaitsForTyping()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeWizard(dir);

        QQmlApplicationEngine engine;
        PageStrings strings;
        engine.rootContext()->setContextObject(&strings);
        engine.addImportPath(dir.path());
        engine.load(QUrl::fromLocalFile(dir.filePath(QStringLiteral("main.qml"))));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        window->show();
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        QObject *keyboard = window->findChild<QObject *>(QStringLiteral("KeyboardPage"));
        QObject *search = window->findChild<QObject *>(QStringLiteral("search"));
        QObject *problem = window->findChild<QObject *>(QStringLiteral("problem"));
        QVERIFY(keyboard && search && problem);

        const QString mismatch = QStringLiteral("The two passwords do not match.");
        keyboard->setProperty("problem", mismatch);
        keyboard->setProperty("complete", false);
        QVERIFY(QMetaObject::invokeMethod(window, "go", Q_ARG(QVariant, 1)));
        QVERIFY(QMetaObject::invokeMethod(search, "forceActiveFocus"));
        QTRY_VERIFY(search->property("activeFocus").toBool());
        QCOMPARE(problem->property("text").toString(), mismatch);

        QString typed;
        for (int i = 0; i < 8; ++i) {
            typed += QLatin1Char('a' + i);
            search->setProperty("text", typed);
            QTest::qWait(400);
            QCOMPARE(problem->property("spokenText").toString(), QString());
        }
        QElapsedTimer clock;
        clock.start();
        QTRY_COMPARE_WITH_TIMEOUT(problem->property("spokenText").toString(), mismatch, 800 + 1500 + 500);
        QVERIFY(clock.elapsed() >= 350);
    }
};

QTEST_MAIN(InstallerTest)

#include "installertest.moc"
