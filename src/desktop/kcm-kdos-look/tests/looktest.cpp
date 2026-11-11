/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   looktest — the KDOS Look page's logic against a fake `kdos`
 *
 * The configuration and data homes are temporary directories, so kdeglobals
 * and the colour schemes the page reads are this test's; fake-kdos plays
 * `kdos theme` and `kdos phosphor` and logs every argument vector it is run
 * with, which is how the tests see exactly what the page asked for. kded-kdos
 * is a fake module on the session bus the test runs under
 * (testing/plasmatest.sh gives each test one with dbus-run-session).
 */

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <functional>

#include "accentmodel.h"
#include "lookcontroller.h"

using Kdos::AccentModel;
using Kdos::LookController;

/* kded-kdos's io.kdos.Kded1, served at org.kde.kded6 /modules/kdos from a
 * connection of its own, so the controller reaches it through the bus as it
 * reaches the real module. */
class FakeKded : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.kdos.Kded1")

public:
    QString state = QStringLiteral("idle");
    QString error;

public Q_SLOTS:
    Q_SCRIPTABLE QString accent() const
    {
        return QStringLiteral("bone");
    }
    Q_SCRIPTABLE QString syncState() const
    {
        return state;
    }
    Q_SCRIPTABLE QString lastError() const
    {
        return error;
    }
    Q_SCRIPTABLE void resync()
    {
    }

Q_SIGNALS:
    Q_SCRIPTABLE void accentSynced(const QString &accent);
    Q_SCRIPTABLE void accentSyncFailed(const QString &accent, const QString &message);
};

namespace
{
const QString KdedConnection = QStringLiteral("fake-kded");
const QStringList Accents = {
    QStringLiteral("phosphor"),
    QStringLiteral("amber"),
    QStringLiteral("ice"),
    QStringLiteral("bone"),
    QStringLiteral("norton"),
    QStringLiteral("borland"),
    QStringLiteral("perfect"),
    QStringLiteral("paper"),
};

QString label(const QString &id)
{
    return id.left(1).toUpper() + id.mid(1);
}

QColor windowOf(int i)
{
    return QColor(10 + i, 20 + i, 30 + i);
}

QColor selectionOf(int i)
{
    return QColor(200 - i, 100 + i, 50 + i);
}

QColor accentOf(int i)
{
    return QColor(250 - i, 200 + i, 20 + i);
}
}

class LookTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir *state = nullptr;
    FakeKded *kded = nullptr;

    /* Serve the fake module; the cases without one see no kded-kdos. */
    void serveKded()
    {
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, KdedConnection);
        QVERIFY(bus.isConnected());
        kded = new FakeKded;
        QVERIFY(bus.registerObject(QStringLiteral("/modules/kdos"), kded, QDBusConnection::ExportScriptableContents));
        QVERIFY(bus.registerService(QStringLiteral("org.kde.kded6")));
    }

    void withdrawKded()
    {
        if (!kded) {
            return;
        }
        QDBusConnection bus(KdedConnection);
        bus.unregisterService(QStringLiteral("org.kde.kded6"));
        bus.unregisterObject(QStringLiteral("/modules/kdos"));
        QDBusConnection::disconnectFromBus(KdedConnection);
        delete kded;
        kded = nullptr;
    }

    QString configHome() const
    {
        return QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
    }

    QString dataHome() const
    {
        return QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
    }

    void writeSchemes()
    {
        const QString dir = dataHome() + QStringLiteral("/color-schemes");
        QDir(dir).removeRecursively();
        QVERIFY(QDir().mkpath(dir));
        for (int i = 0; i < Accents.size(); ++i) {
            KConfig c(dir + QStringLiteral("/KDOS%1.colors").arg(label(Accents.at(i))), KConfig::SimpleConfig);
            KConfigGroup(&c, QStringLiteral("General")).writeEntry("Name", QString(QStringLiteral("KDOS ") + label(Accents.at(i))));
            KConfigGroup(&c, QStringLiteral("Colors:Window")).writeEntry("BackgroundNormal", windowOf(i));
            KConfigGroup(&c, QStringLiteral("Colors:Window")).writeEntry("ForegroundNormal", QColor(240, 240, 240));
            KConfigGroup(&c, QStringLiteral("Colors:View")).writeEntry("BackgroundNormal", windowOf(i).darker());
            KConfigGroup(&c, QStringLiteral("Colors:View")).writeEntry("DecorationFocus", accentOf(i));
            KConfigGroup(&c, QStringLiteral("Colors:Selection")).writeEntry("BackgroundNormal", selectionOf(i));
            KConfigGroup(&c, QStringLiteral("Colors:Selection")).writeEntry("ForegroundNormal", QColor(0, 0, 0));
            QVERIFY(c.sync());
        }
    }

    void writePlasma(const QString &scheme, const QString &anim = QString())
    {
        QFile f(configHome() + QStringLiteral("/kdeglobals"));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("[General]\nColorScheme=" + scheme.toUtf8() + "\n");
        if (!anim.isEmpty()) {
            f.write("\n[KDE]\nAnimationDurationFactor=" + anim.toUtf8() + "\n");
        }
    }

    void writeState(const QString &file, const QString &value)
    {
        QFile f(state->filePath(file));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(value.toUtf8());
    }

    QString plasmaScheme()
    {
        KConfig c(configHome() + QStringLiteral("/kdeglobals"), KConfig::SimpleConfig);
        return KConfigGroup(&c, QStringLiteral("General")).readEntry("ColorScheme", QString());
    }

    /* The last command the page ran that was not a read-back. */
    QString lastAction()
    {
        const QStringList all = argv();
        for (auto it = all.crbegin(); it != all.crend(); ++it) {
            if (*it != QLatin1String("theme list") && *it != QLatin1String("phosphor status --json")) {
                return *it;
            }
        }
        return {};
    }

    QStringList argv()
    {
        QFile f(state->filePath(QStringLiteral("argv.log")));
        if (!f.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

    /* A greeter set as kdos-powerd copies it: a light accent's names its
     * dark partner's scheme. */
    void writeGreeter(const QString &accent, const QString &scheme)
    {
        QVERIFY(QDir().mkpath(state->filePath(QStringLiteral("greeter/") + accent)));
        writeState(QStringLiteral("greeter/") + accent + QStringLiteral("/kdeglobals"), QStringLiteral("[General]\nColorScheme=") + scheme + QLatin1Char('\n'));
    }

    LookController::Paths paths() const
    {
        LookController::Paths p;
        p.program = QStringLiteral(KDOS_FAKE_KDOS);
        p.machineAccent = state->filePath(QStringLiteral("etc/accent"));
        p.greeterDir = state->filePath(QStringLiteral("greeter"));
        p.esp = state->filePath(QStringLiteral("esp"));
        return p;
    }

    void refreshed(LookController &c)
    {
        QSignalSpy phosphor(&c, &LookController::phosphorChanged);
        c.refresh();
        QVERIFY(phosphor.wait());
        QTRY_VERIFY(!c.busy());
    }

    /* The same, while kded-kdos says it runs, which keeps the page busy. */
    void refreshedWhileSyncing(LookController &c)
    {
        QSignalSpy phosphor(&c, &LookController::phosphorChanged);
        c.refresh();
        QVERIFY(phosphor.wait());
        QTRY_VERIFY(c.syncing());
    }

    bool act(LookController &c, const std::function<bool()> &what)
    {
        QSignalSpy done(&c, &LookController::commandDone);
        if (!what()) {
            return false;
        }
        if (!done.wait(10000)) {
            return false;
        }
        return done.first().at(1).toBool();
    }

private Q_SLOTS:
    void init()
    {
        state = new QTemporaryDir;
        QVERIFY(state->isValid());
        qputenv("FAKE_KDOS_DIR", state->path().toLocal8Bit());
        qunsetenv("FAKE_KDOS_NO_MACHINE");
        writeSchemes();
        writePlasma(QStringLiteral("KDOSBone"));
        QVERIFY(QDir().mkpath(state->filePath(QStringLiteral("etc"))));
        writeState(QStringLiteral("etc/accent"), QStringLiteral("bone"));
        QVERIFY(QDir().mkpath(state->filePath(QStringLiteral("esp"))));
        writeGreeter(QStringLiteral("bone"), QStringLiteral("KDOSBone"));
        writeGreeter(QStringLiteral("amber"), QStringLiteral("KDOSAmber"));
        writeGreeter(QStringLiteral("paper"), QStringLiteral("KDOSBone"));
    }

    void cleanup()
    {
        withdrawKded();
        delete state;
        state = nullptr;
    }

    void withoutKdedKdosThePageActsAlone()
    {
        QVERIFY(QDBusConnection::sessionBus().isConnected());
        LookController c(paths());
        refreshed(c);
        QVERIFY(!c.syncing());
        QVERIFY(c.syncError().isEmpty());
        QVERIFY(act(c, [&] {
            return c.applyAccent(QStringLiteral("amber"));
        }));
    }

    void aRunOfKdedKdosHoldsThePage()
    {
        serveKded();
        kded->state = QStringLiteral("running");
        writePlasma(QStringLiteral("KDOSAmber"));
        LookController c(paths());
        QSignalSpy phosphor(&c, &LookController::phosphorChanged);
        c.refresh();
        QVERIFY(phosphor.wait());
        QTRY_VERIFY(c.syncing());
        QVERIFY(c.busy());
        QVERIFY(c.busyText().contains(QStringLiteral("Amber")));
        QVERIFY(!c.canSync());
        QVERIFY(!c.applyAccent(QStringLiteral("ice")));
        QVERIFY(!c.setPhosphor(true));
        QVERIFY(!c.syncNow());
        QVERIFY(!argv().contains(QStringLiteral("theme ice")));

        /* The module carried amber down; the page reads the result back. */
        const int reads = int(argv().count(QStringLiteral("theme list")));
        writeState(QStringLiteral("accent"), QStringLiteral("amber"));
        writeState(QStringLiteral("etc/accent"), QStringLiteral("amber"));
        kded->state = QStringLiteral("idle");
        Q_EMIT kded->accentSynced(QStringLiteral("amber"));
        QTRY_VERIFY(!c.syncing());
        QTRY_VERIFY(argv().count(QStringLiteral("theme list")) > reads);
        QTRY_COMPARE(c.accent(), QStringLiteral("amber"));
        QTRY_VERIFY(!c.busy());
        QCOMPARE(c.syncState(), LookController::SyncState::InStep);
        QVERIFY(c.applyAccent(QStringLiteral("ice")));
    }

    void aRunThatEndedUnheardIsStillAnEnd()
    {
        serveKded();
        kded->state = QStringLiteral("running");
        LookController c(paths());
        refreshedWhileSyncing(c);
        /* kded6 went away mid-run: no signal, and the next question finds
         * nobody, which is not a run still going. */
        withdrawKded();
        QTRY_VERIFY_WITH_TIMEOUT(!c.syncing(), 8000);
        QTRY_VERIFY(!c.busy());
    }

    void aFailedRunIsSaidWhileTheAccentsDisagree()
    {
        serveKded();
        kded->state = QStringLiteral("failed");
        kded->error = QStringLiteral("boot menu, splash and login screen unchanged (kdos-powerd did not answer)");
        writePlasma(QStringLiteral("KDOSAmber"));
        LookController c(paths());
        refreshed(c);
        QCOMPARE(c.syncState(), LookController::SyncState::PlasmaAhead);
        QTRY_VERIFY(c.syncError().contains(QStringLiteral("kdos-powerd did not answer")));
        QVERIFY(c.outOfStep());
        QVERIFY(c.canSync());

        /* Put right by the page's own repair: the module's stale failure is
         * no longer the state, because nothing disagrees. */
        QVERIFY(act(c, [&] {
            return c.syncNow();
        }));
        QCOMPARE(c.syncState(), LookController::SyncState::InStep);
        QVERIFY(c.syncError().isEmpty());
    }

    void aFailureSignalIsShownAtOnce()
    {
        serveKded();
        writePlasma(QStringLiteral("KDOSNorton"));
        LookController c(paths());
        refreshed(c);
        QVERIFY(c.syncError().isEmpty());
        kded->state = QStringLiteral("failed");
        Q_EMIT kded->accentSyncFailed(QStringLiteral("norton"), QStringLiteral("kdos theme exited with status 1."));
        QTRY_VERIFY(c.syncError().contains(QStringLiteral("status 1")));
    }

    void listsTheAccentsInKdosOrderWithTheirOwnColours()
    {
        LookController c(paths());
        refreshed(c);
        AccentModel *m = c.accents();
        QCOMPARE(m->rowCount(), 8);
        for (int i = 0; i < Accents.size(); ++i) {
            const QModelIndex idx = m->index(i);
            QCOMPARE(m->data(idx, AccentModel::AccentIdRole).toString(), Accents.at(i));
            QCOMPARE(m->data(idx, AccentModel::LabelRole).toString(), label(Accents.at(i)));
            QCOMPARE(m->data(idx, AccentModel::SchemeRole).toString(), QStringLiteral("KDOS") + label(Accents.at(i)));
            QVERIFY(m->data(idx, AccentModel::AvailableRole).toBool());
            QCOMPARE(m->data(idx, AccentModel::WindowColorRole).value<QColor>(), windowOf(i));
            QCOMPARE(m->data(idx, AccentModel::AccentColorRole).value<QColor>(), accentOf(i));
            QCOMPARE(m->data(idx, AccentModel::CurrentRole).toBool(), Accents.at(i) == QLatin1String("bone"));
        }
        QCOMPARE(c.accent(), QStringLiteral("bone"));
        QCOMPARE(c.syncState(), LookController::SyncState::InStep);
        QVERIFY(!c.canSync());
        QVERIFY(c.syncText().contains(QStringLiteral("Bone")));
    }

    void eachAccentRepaintsPlasmaInOneAction()
    {
        LookController c(paths());
        refreshed(c);
        for (const QString &id : Accents) {
            QVERIFY2(act(c, [&] {
                         return c.applyAccent(id);
                     }),
                     qPrintable(id));
            QCOMPARE(lastAction(), QStringLiteral("theme ") + id);
            QCOMPARE(plasmaScheme(), QStringLiteral("KDOS") + label(id));
            QCOMPARE(c.accent(), id);
            QCOMPARE(c.plasmaScheme(), QStringLiteral("KDOS") + label(id));
            QCOMPARE(c.syncState(), LookController::SyncState::InStep);
        }
    }

    void aMissingSchemeIsListedButNotApplied()
    {
        QVERIFY(QFile::remove(dataHome() + QStringLiteral("/color-schemes/KDOSIce.colors")));
        LookController c(paths());
        refreshed(c);
        const QModelIndex ice = c.accents()->index(2);
        QCOMPARE(c.accents()->data(ice, AccentModel::AccentIdRole).toString(), QStringLiteral("ice"));
        QVERIFY(!c.accents()->data(ice, AccentModel::AvailableRole).toBool());
        QVERIFY(!c.applyAccent(QStringLiteral("ice")));
        QVERIFY(!argv().contains(QStringLiteral("theme ice")));
    }

    void aNameNotInTheListIsNeverRun()
    {
        LookController c(paths());
        refreshed(c);
        QVERIFY(!c.applyAccent(QStringLiteral("bone; rm -rf ~")));
        QVERIFY(!c.applyAccent(QStringLiteral("--audit")));
        for (const QString &line : argv()) {
            QVERIFY2(line == QLatin1String("theme list") || line == QLatin1String("phosphor status --json"), qPrintable(line));
        }
    }

    void refusesASecondClickWhileOneRuns()
    {
        LookController c(paths());
        refreshed(c);
        QSignalSpy done(&c, &LookController::commandDone);
        QVERIFY(c.applyAccent(QStringLiteral("amber")));
        QVERIFY(!c.applyAccent(QStringLiteral("ice")));
        QVERIFY(done.wait());
        QVERIFY(!argv().contains(QStringLiteral("theme ice")));
    }

    void plasmaAheadIsCarriedDownWithoutReapplyingPlasma()
    {
        writePlasma(QStringLiteral("KDOSAmber"));
        LookController c(paths());
        refreshed(c);
        QCOMPARE(c.syncState(), LookController::SyncState::PlasmaAhead);
        QVERIFY(c.canSync());
        QVERIFY(c.syncText().contains(QStringLiteral("Amber")));
        QVERIFY(c.syncText().contains(QStringLiteral("Bone")));
        QVERIFY(act(c, [&] {
            return c.syncNow();
        }));
        QCOMPARE(lastAction(), QStringLiteral("theme --from-plasma amber"));
        QCOMPARE(c.accent(), QStringLiteral("amber"));
        QCOMPARE(c.syncState(), LookController::SyncState::InStep);
    }

    void aLoginScreenInTheOldColoursIsShown()
    {
        writeState(QStringLiteral("etc/accent"), QStringLiteral("ice"));
        qputenv("FAKE_KDOS_NO_MACHINE", "1");
        LookController c(paths());
        refreshed(c);
        QCOMPARE(c.syncState(), LookController::SyncState::MachineBehind);
        QVERIFY(c.syncText().contains(QStringLiteral("Ice")));
        QVERIFY(act(c, [&] {
            return c.syncNow();
        }));
        QCOMPARE(lastAction(), QStringLiteral("theme --from-plasma bone"));
        /* kdos-powerd did not answer: still behind, and still said. */
        QCOMPARE(c.syncState(), LookController::SyncState::MachineBehind);
        qunsetenv("FAKE_KDOS_NO_MACHINE");
        QVERIFY(act(c, [&] {
            return c.syncNow();
        }));
        QCOMPARE(c.syncState(), LookController::SyncState::InStep);
    }

    void aSchemeThatIsNotKdosIsLeftAlone()
    {
        writePlasma(QStringLiteral("BreezeDark"));
        LookController c(paths());
        refreshed(c);
        QCOMPARE(c.syncState(), LookController::SyncState::NotKdos);
        QVERIFY(!c.canSync());
        QVERIFY(!c.syncNow());
        QVERIFY(c.syncText().contains(QStringLiteral("BreezeDark")));
    }

    void followsAPickMadeElsewhere()
    {
        LookController c(paths());
        refreshed(c);
        QCOMPARE(c.syncState(), LookController::SyncState::InStep);
        writePlasma(QStringLiteral("KDOSNorton"));
        writeState(QStringLiteral("accent"), QStringLiteral("norton"));
        /* kdos-powerd writes the machine's accent by a rename, which is what
         * the watched directory sees. */
        writeState(QStringLiteral("accent.new"), QStringLiteral("norton"));
        QVERIFY(QFile::remove(state->filePath(QStringLiteral("etc/accent"))));
        QVERIFY(QFile::rename(state->filePath(QStringLiteral("accent.new")), state->filePath(QStringLiteral("etc/accent"))));
        QTRY_COMPARE_WITH_TIMEOUT(c.accent(), QStringLiteral("norton"), 5000);
        QCOMPARE(c.syncState(), LookController::SyncState::InStep);
    }

    void phosphorPairsARetroAccentAndPutsItBack()
    {
        LookController c(paths());
        refreshed(c);
        QVERIFY(c.phosphorKnown());
        QVERIFY(c.phosphorSupported());
        QVERIFY(!c.phosphorOn());
        QVERIFY(act(c, [&] {
            return c.setPhosphor(true);
        }));
        QCOMPARE(lastAction(), QStringLiteral("phosphor on"));
        QVERIFY(c.phosphorOn());
        QCOMPARE(c.accent(), QStringLiteral("phosphor"));
        QCOMPARE(plasmaScheme(), QStringLiteral("KDOSPhosphor"));
        QVERIFY(act(c, [&] {
            return c.setPhosphor(false);
        }));
        QCOMPARE(lastAction(), QStringLiteral("phosphor off"));
        QVERIFY(!c.phosphorOn());
        QCOMPARE(c.accent(), QStringLiteral("bone"));
        QCOMPARE(plasmaScheme(), QStringLiteral("KDOSBone"));
    }

    void phosphorOnSoftwareRenderingIsRefused()
    {
        writeState(QStringLiteral("unsupported"), QStringLiteral("1"));
        LookController c(paths());
        refreshed(c);
        QVERIFY(c.phosphorKnown());
        QVERIFY(!c.phosphorSupported());
        QVERIFY(!c.phosphorUsable());
        QCOMPARE(c.phosphorReason(), QStringLiteral("Phosphor mode needs GPU rendering, and this session renders in software."));
        QVERIFY(!c.setPhosphor(true));
        QVERIFY(!argv().contains(QStringLiteral("phosphor on")));
    }

    void noOptionIsRecordedForAModeThatCannotComeOn()
    {
        writeState(QStringLiteral("unsupported"), QStringLiteral("1"));
        LookController c(paths());
        refreshed(c);
        QVERIFY(!c.setPreset(QStringLiteral("retro")));
        QVERIFY(!c.setTerminus(true));
        QVERIFY(!c.setFullscreen(true));
        for (const QString &line : argv()) {
            QVERIFY2(line == QLatin1String("theme list") || line == QLatin1String("phosphor status --json"), qPrintable(line));
        }
    }

    void aModeAlreadyOnKeepsItsOptions()
    {
        writeState(QStringLiteral("unsupported"), QStringLiteral("1"));
        writeState(QStringLiteral("phosphor"), QStringLiteral("on"));
        LookController c(paths());
        refreshed(c);
        QVERIFY(c.phosphorUsable());
        QVERIFY(act(c, [&] {
            return c.setTerminus(true);
        }));
        QCOMPARE(lastAction(), QStringLiteral("phosphor terminus on"));
    }

    void aMissingSessionIsSaidInThePagesWords()
    {
        writeState(QStringLiteral("nosession"), QStringLiteral("1"));
        LookController c(paths());
        refreshed(c);
        QVERIFY(!c.phosphorSupported());
        QCOMPARE(c.phosphorReason(), QStringLiteral("Phosphor mode needs a running Plasma session."));
    }

    void theLoginScreenWearsALightAccentsDarkPartner()
    {
        writePlasma(QStringLiteral("KDOSPaper"));
        writeState(QStringLiteral("accent"), QStringLiteral("paper"));
        writeState(QStringLiteral("etc/accent"), QStringLiteral("paper"));
        LookController c(paths());
        refreshed(c);
        QCOMPARE(c.syncState(), LookController::SyncState::InStep);
        QCOMPARE(c.loginAccent(), QStringLiteral("bone"));
        QCOMPARE(c.syncText(), QStringLiteral("Plasma, the terminals, the boot menu and the splash wear Paper, and the login screen its dark partner Bone."));
    }

    void aDarkAccentNamesNoPartner()
    {
        writePlasma(QStringLiteral("KDOSAmber"));
        writeState(QStringLiteral("accent"), QStringLiteral("amber"));
        writeState(QStringLiteral("etc/accent"), QStringLiteral("amber"));
        LookController c(paths());
        refreshed(c);
        QCOMPARE(c.syncText(), QStringLiteral("Plasma, the terminals, the boot menu, the splash and the login screen all wear Amber."));
    }

    void aMachineWithNoBootMenuDoesNotNameOne()
    {
        QVERIFY(QDir(state->filePath(QStringLiteral("esp"))).removeRecursively());
        LookController c(paths());
        refreshed(c);
        QVERIFY(!c.bootMenu());
        QCOMPARE(c.syncText(), QStringLiteral("Plasma, the terminals, the splash and the login screen all wear Bone."));
        writePlasma(QStringLiteral("KDOSPaper"));
        writeState(QStringLiteral("accent"), QStringLiteral("paper"));
        writeState(QStringLiteral("etc/accent"), QStringLiteral("paper"));
        refreshed(c);
        QCOMPARE(c.syncText(), QStringLiteral("Plasma, the terminals and the splash wear Paper, and the login screen its dark partner Bone."));
    }

    void aRefusalIsShownInTheCommandsWords()
    {
        LookController c(paths());
        refreshed(c);
        QVERIFY(c.phosphorSupported());
        /* The session lost its GPU after the page read the status. */
        writeState(QStringLiteral("unsupported"), QStringLiteral("1"));
        QVERIFY(!act(c, [&] {
            return c.setPhosphor(true);
        }));
        QCOMPARE(c.errorMessage(), QStringLiteral("phosphor mode needs GPU rendering (this session renders in software)"));
        QVERIFY(!c.phosphorOn());
        QVERIFY(!c.phosphorSupported());
        c.dismissError();
        QVERIFY(c.errorMessage().isEmpty());
    }

    void presetAndTerminusAreTheCommandsToo()
    {
        LookController c(paths());
        refreshed(c);
        QCOMPARE(c.preset(), QStringLiteral("subtle"));
        QVERIFY(act(c, [&] {
            return c.setPreset(QStringLiteral("retro"));
        }));
        QCOMPARE(lastAction(), QStringLiteral("phosphor preset retro"));
        QCOMPARE(c.preset(), QStringLiteral("retro"));
        QVERIFY(!c.setPreset(QStringLiteral("loud")));
        QVERIFY(act(c, [&] {
            return c.setTerminus(true);
        }));
        QCOMPARE(lastAction(), QStringLiteral("phosphor terminus on"));
        QVERIFY(c.terminus());
        QVERIFY(!c.fullscreen());
        QVERIFY(act(c, [&] {
            return c.setFullscreen(true);
        }));
        QCOMPARE(lastAction(), QStringLiteral("phosphor fullscreen on"));
        QVERIFY(c.fullscreen());
    }

    void aCustomTuningIsNeitherPreset()
    {
        writeState(QStringLiteral("preset"), QStringLiteral("custom"));
        LookController c(paths());
        refreshed(c);
        QCOMPARE(c.preset(), QStringLiteral("custom"));
    }

    void reducedMotionIsNoticed()
    {
        writePlasma(QStringLiteral("KDOSBone"), QStringLiteral("0"));
        LookController c(paths());
        refreshed(c);
        QVERIFY(c.reducedMotion());
        writePlasma(QStringLiteral("KDOSBone"), QStringLiteral("0.5"));
        refreshed(c);
        QVERIFY(!c.reducedMotion());
    }

    void aMissingCommandIsSaid()
    {
        LookController::Paths p = paths();
        p.program = state->filePath(QStringLiteral("no-such-kdos"));
        LookController c(p);
        refreshed(c);
        QVERIFY(!c.errorMessage().isEmpty());
        QCOMPARE(c.accents()->rowCount(), 0);
        QCOMPARE(c.syncState(), LookController::SyncState::Unknown);
        QVERIFY(!c.phosphorKnown());
    }
};

int main(int argc, char **argv)
{
    /* Before anything reads a standard path: the homes are this run's, and
     * the system directories are empty, so no installed kdeglobals or scheme
     * answers in place of a fixture. */
    QTemporaryDir homes;
    if (!homes.isValid()) {
        return 1;
    }
    for (const char *sub : {"config", "data", "etc-xdg", "share"}) {
        QDir().mkpath(homes.filePath(QString::fromLatin1(sub)));
    }
    qputenv("XDG_CONFIG_HOME", homes.filePath(QStringLiteral("config")).toLocal8Bit());
    qputenv("XDG_DATA_HOME", homes.filePath(QStringLiteral("data")).toLocal8Bit());
    qputenv("XDG_CONFIG_DIRS", homes.filePath(QStringLiteral("etc-xdg")).toLocal8Bit());
    qputenv("XDG_DATA_DIRS", homes.filePath(QStringLiteral("share")).toLocal8Bit());
    QGuiApplication app(argc, argv);
    LookTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "looktest.moc"
