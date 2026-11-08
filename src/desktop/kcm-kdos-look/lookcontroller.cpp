/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   lookcontroller — the accent everywhere, and Phosphor mode
 */

#include "lookcontroller.h"

#include <KCMultiDialog>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

namespace Kdos
{

namespace
{
const QStringList ThemeList = {QStringLiteral("theme"), QStringLiteral("list")};
const QStringList PhosphorStatus = {QStringLiteral("phosphor"), QStringLiteral("status"), QStringLiteral("--json")};

/* The effect's settings page, which the kdos-crt patch installs into KWin. */
KPluginMetaData fineTunePlugin()
{
    return KPluginMetaData::findPluginById(QStringLiteral("kwin/effects/configs"), QStringLiteral("kwin_kdos_crt_config"));
}

/* A file another program rewrites lands as a rename into its directory, so
 * the directory is what is watched; the pause lets a run that writes several
 * files finish before the page reads them. */
constexpr int SettleMs = 400;

QString userAccentDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/kdos");
}
}

LookController::LookController(const Paths &paths, QObject *parent)
    : LookController(paths, QDBusConnection::sessionBus(), parent)
{
}

LookController::LookController(const Paths &paths, const QDBusConnection &session, QObject *parent)
    : QObject(parent)
    , m_command(new KdosCommand(paths.program, this))
    , m_kded(new KdedClient(session, this))
    , m_accents(new AccentModel(this))
    , m_paths(paths)
    , m_files(new QFileSystemWatcher(this))
    , m_debounce(new QTimer(this))
{
    connect(m_command, &KdosCommand::finished, this, &LookController::onFinished);
    connect(m_command, &KdosCommand::busyChanged, this, [this] {
        if (!m_command->busy()) {
            m_busyText.clear();
        }
        Q_EMIT busyChanged();
    });

    m_debounce->setSingleShot(true);
    m_debounce->setInterval(SettleMs);
    connect(m_debounce, &QTimer::timeout, this, &LookController::refresh);

    connect(m_kded, &KdedClient::stateChanged, this, [this] {
        Q_EMIT busyChanged();
        Q_EMIT stateChanged();
    });
    /* A run of kded-kdos's ended: what it wrote is read back as any change
     * made elsewhere is. */
    connect(m_kded, &KdedClient::runEnded, m_debounce, qOverload<>(&QTimer::start));

    m_watcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kdeglobals")));
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group, const QByteArrayList &names) {
        if ((group.name() == QLatin1String("General") && names.contains("ColorScheme"))
            || (group.name() == QLatin1String("KDE") && names.contains("AnimationDurationFactor"))) {
            readPlasma();
            Q_EMIT stateChanged();
            /* kded-kdos follows a pick made on the Global Theme page within a
             * second or two; looking again then shows where it landed. */
            m_debounce->start();
        }
    });

    for (const QString &dir : {QFileInfo(m_paths.machineAccent).absolutePath(), userAccentDir()}) {
        if (QFileInfo(dir).isDir()) {
            m_files->addPath(dir);
        }
    }
    connect(m_files, &QFileSystemWatcher::directoryChanged, this, [this] {
        if (!m_pendingAction) {
            m_debounce->start();
        }
    });
}

AccentModel *LookController::accents() const
{
    return m_accents;
}

bool LookController::busy() const
{
    return m_command->busy() || syncing();
}

QString LookController::busyText() const
{
    if (m_command->busy() || !syncing()) {
        return m_busyText;
    }
    const QString plasma = m_accents->accentOfScheme(m_plasmaScheme);
    if (plasma.isEmpty()) {
        return i18nc("@info:status", "Carrying Plasma's accent to the terminals and the screens before login…");
    }
    return i18nc("@info:status %1 is an accent", "Carrying %1 to the terminals and the screens before login…", m_accents->labelOf(plasma));
}

bool LookController::syncing() const
{
    return m_kded->state() == KdedClient::State::Running;
}

bool LookController::canAct() const
{
    return !m_pendingAction && !syncing();
}

QString LookController::errorMessage() const
{
    return m_error;
}

bool LookController::commandFound() const
{
    return !m_command->program().isEmpty();
}

QString LookController::accent() const
{
    return m_accent;
}

QString LookController::plasmaScheme() const
{
    return m_plasmaScheme;
}

QString LookController::machineAccent() const
{
    return m_machineAccent;
}

LookController::SyncState LookController::syncState() const
{
    if (m_accents->count() == 0 || m_accent.isEmpty() || m_plasmaScheme.isEmpty()) {
        return SyncState::Unknown;
    }
    const QString plasma = m_accents->accentOfScheme(m_plasmaScheme);
    if (plasma.isEmpty()) {
        return SyncState::NotKdos;
    }
    if (plasma != m_accent) {
        return SyncState::PlasmaAhead;
    }
    if (!m_machineAccent.isEmpty() && m_machineAccent != m_accent) {
        return SyncState::MachineBehind;
    }
    return SyncState::InStep;
}

QString LookController::syncText() const
{
    const QString label = m_accents->labelOf(m_accent);
    switch (syncState()) {
    case SyncState::InStep:
        if (m_machineAccent.isEmpty()) {
            return i18nc("@info %1 is an accent", "Plasma and the terminals wear %1.", label);
        }
        return i18nc("@info %1 is an accent", "Plasma, the terminals, the boot menu, the splash and the login screen all wear %1.", label);
    case SyncState::PlasmaAhead:
        return i18nc("@info %1 and %2 are accents",
                     "Plasma wears %1, but the terminals and the screens before login still wear %2.",
                     m_accents->labelOf(m_accents->accentOfScheme(m_plasmaScheme)),
                     label);
    case SyncState::MachineBehind:
        return i18nc("@info %1 is an accent", "The boot menu, the splash and the login screen still wear %1.", m_accents->labelOf(m_machineAccent));
    case SyncState::NotKdos:
        return i18nc("@info %1 is a color scheme's name, %2 an accent",
                     "Plasma uses the color scheme “%1”, which is not a KDOS accent, so the terminals and the screens before login keep %2.",
                     m_plasmaScheme,
                     label);
    case SyncState::Unknown:
        break;
    }
    if (!commandFound()) {
        return i18nc("@info", "The kdos command is not installed, so no accent can be applied from here.");
    }
    return {};
}

bool LookController::outOfStep() const
{
    const SyncState s = syncState();
    return s == SyncState::PlasmaAhead || s == SyncState::MachineBehind;
}

QString LookController::syncError() const
{
    if (m_kded->state() != KdedClient::State::Failed || !outOfStep()) {
        return {};
    }
    if (m_kded->lastError().isEmpty()) {
        return i18nc("@info", "The last attempt to carry the accent below the desktop failed.");
    }
    return i18nc("@info %1 is the reason kdos theme gave", "The last attempt to carry the accent below the desktop failed: %1", m_kded->lastError());
}

bool LookController::canSync() const
{
    return canAct() && outOfStep();
}

bool LookController::phosphorKnown() const
{
    return m_phosphorKnown;
}

bool LookController::phosphorOn() const
{
    return m_phosphorOn;
}

bool LookController::phosphorSupported() const
{
    return m_phosphorSupported;
}

QString LookController::phosphorReason() const
{
    return m_phosphorReason;
}

QString LookController::preset() const
{
    return m_preset;
}

bool LookController::terminus() const
{
    return m_terminus;
}

bool LookController::fullscreen() const
{
    return m_fullscreen;
}

bool LookController::fineTuneAvailable() const
{
    return fineTunePlugin().isValid();
}

bool LookController::reducedMotion() const
{
    return m_reducedMotion;
}

void LookController::refresh()
{
    readPlasma();
    readMachine();
    m_kded->query();
    Q_EMIT stateChanged();
    if (m_refreshQueued) {
        return;
    }
    m_refreshQueued = true;
    m_command->run(ThemeList);
    m_command->run(PhosphorStatus);
}

void LookController::act(const QStringList &arguments, const QString &busyText)
{
    m_pendingAction = true;
    m_busyText = busyText;
    setError({});
    Q_EMIT stateChanged();
    m_command->run(arguments);
}

bool LookController::applyAccent(const QString &accent)
{
    if (!canAct() || !m_accents->isAvailable(accent)) {
        return false;
    }
    act({QStringLiteral("theme"), accent}, i18nc("@info:status %1 is an accent", "Applying %1…", m_accents->labelOf(accent)));
    return true;
}

bool LookController::syncNow()
{
    const QString plasma = m_accents->accentOfScheme(m_plasmaScheme);
    if (!canSync() || plasma.isEmpty()) {
        return false;
    }
    act({QStringLiteral("theme"), QStringLiteral("--from-plasma"), plasma},
        i18nc("@info:status %1 is an accent", "Repainting everything in %1…", m_accents->labelOf(plasma)));
    return true;
}

bool LookController::setPhosphor(bool on)
{
    if (!canAct() || (on && !m_phosphorSupported)) {
        return false;
    }
    act({QStringLiteral("phosphor"), on ? QStringLiteral("on") : QStringLiteral("off")},
        on ? i18nc("@info:status", "Turning Phosphor mode on…") : i18nc("@info:status", "Turning Phosphor mode off…"));
    return true;
}

bool LookController::setPreset(const QString &preset)
{
    if (!canAct() || (preset != QLatin1String("subtle") && preset != QLatin1String("retro"))) {
        return false;
    }
    act({QStringLiteral("phosphor"), QStringLiteral("preset"), preset}, i18nc("@info:status", "Changing the preset…"));
    return true;
}

bool LookController::setTerminus(bool on)
{
    if (!canAct()) {
        return false;
    }
    act({QStringLiteral("phosphor"), QStringLiteral("terminus"), on ? QStringLiteral("on") : QStringLiteral("off")},
        i18nc("@info:status", "Changing the terminal font…"));
    return true;
}

bool LookController::setFullscreen(bool on)
{
    if (!canAct()) {
        return false;
    }
    act({QStringLiteral("phosphor"), QStringLiteral("fullscreen"), on ? QStringLiteral("on") : QStringLiteral("off")},
        i18nc("@info:status", "Changing fullscreen windows…"));
    return true;
}

void LookController::openFineTune()
{
    const KPluginMetaData md = fineTunePlugin();
    if (!md.isValid()) {
        return;
    }
    auto *dialog = new KCMultiDialog();
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window", "Phosphor Mode Tuning"));
    dialog->addModule(md);
    /* The effect watches its own group; the page reads the result back,
     * which is "custom" unless the values are a preset's. */
    connect(dialog, &QObject::destroyed, this, &LookController::refresh);
    dialog->show();
}

void LookController::dismissError()
{
    setError({});
}

void LookController::onFinished(const KdosCommand::Result &result)
{
    if (result.arguments == ThemeList) {
        if (result.started && result.exitCode == 0) {
            m_accent = m_accents->setFromList(result.out);
        } else {
            setError(failureText(result));
        }
        Q_EMIT stateChanged();
        return;
    }
    if (result.arguments == PhosphorStatus) {
        m_refreshQueued = false;
        if (result.started && result.exitCode == 0) {
            parsePhosphor(result.out);
        } else {
            m_phosphorKnown = false;
        }
        Q_EMIT phosphorChanged();
        if (m_pendingAction && !m_actionArguments.isEmpty()) {
            const QStringList done = m_actionArguments;
            const bool ok = m_actionOk;
            m_actionArguments.clear();
            m_pendingAction = false;
            Q_EMIT stateChanged();
            Q_EMIT commandDone(done, ok);
        }
        return;
    }

    /* An action. Whatever it reported, the files it wrote are read back:
     * a run that failed half way still changed what it got to. */
    m_actionArguments = result.arguments;
    m_actionOk = result.started && result.exitCode == 0;
    if (!m_actionOk) {
        setError(failureText(result));
    }
    m_refreshQueued = false;
    refresh();
}

void LookController::readPlasma()
{
    KSharedConfigPtr globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    globals->reparseConfiguration();
    m_plasmaScheme = KConfigGroup(globals, QStringLiteral("General")).readEntry("ColorScheme", QString());
    m_reducedMotion = KConfigGroup(globals, QStringLiteral("KDE")).readEntry("AnimationDurationFactor", 1.0) <= 0.0;
}

void LookController::readMachine()
{
    static const QRegularExpression name(QStringLiteral("^[a-z][a-z0-9-]{0,30}$"));
    QFile f(m_paths.machineAccent);
    QString accent;
    if (f.open(QIODevice::ReadOnly)) {
        accent = QString::fromUtf8(f.readLine(64)).trimmed();
    }
    m_machineAccent = name.match(accent).hasMatch() ? accent : QString();
}

void LookController::parsePhosphor(const QString &json)
{
    /* {"on", "supported", "reason", "preset", "terminus", "fullscreen",
     * "previous_accent"}, the record the welcome tour's Look page reads too.
     * A reply whose "on" is not a boolean is not one this page understands,
     * and offers nothing. "preset" is subtle, retro or custom. */
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    const QJsonValue on = o.value(QStringLiteral("on"));
    m_phosphorKnown = on.isBool();
    m_phosphorOn = on.toBool();
    m_phosphorSupported = o.value(QStringLiteral("supported")).toBool();
    m_phosphorReason = o.value(QStringLiteral("reason")).toString();
    const QString preset = o.value(QStringLiteral("preset")).toString();
    m_preset = preset == QLatin1String("retro") || preset == QLatin1String("custom") ? preset : QStringLiteral("subtle");
    m_terminus = o.value(QStringLiteral("terminus")).toBool();
    m_fullscreen = o.value(QStringLiteral("fullscreen")).toBool();
}

void LookController::setError(const QString &message)
{
    if (m_error == message) {
        return;
    }
    m_error = message;
    Q_EMIT errorMessageChanged();
}

QString LookController::failureText(const KdosCommand::Result &result)
{
    const QString command = QStringLiteral("kdos ") + result.arguments.join(QLatin1Char(' '));
    if (!result.started) {
        if (result.err.isEmpty()) {
            return i18nc("@info", "The kdos command is not installed.");
        }
        return i18nc("@info %1 is a command, %2 the reason", "“%1” could not be run: %2", command, result.err);
    }
    for (const QString &text : {result.err, result.out}) {
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            return lines.constLast().trimmed();
        }
    }
    return i18nc("@info %1 is a command, %2 a number", "“%1” failed with exit status %2.", command, result.exitCode);
}

} // namespace Kdos
