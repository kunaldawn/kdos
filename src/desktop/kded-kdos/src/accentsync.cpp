/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — the accent, kept in step with Plasma
 * ---------------------------------
 */

#include "accentsync.h"
#include "debug.h"
#include "logic.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KNotification>

#include <QFileInfo>
#include <QStandardPaths>

namespace Kdos
{

/* A global theme writes kdeglobals in several notified syncs; the scheme is
 * read once they have settled, not once per sync. */
static constexpr int DEBOUNCE_MS = 300;

/* `kdos theme` restamps the boot menu through kdos-powerd and rewrites a
 * dozen terminal programs' files; a run still going after this long is
 * waiting on something that will not answer, and is stopped so the next
 * change can run. */
static constexpr int TIMEOUT_MS = 60 * 1000;

AccentSync::AccentSync(QObject *parent)
    : QObject(parent)
    , m_globals(KSharedConfig::openConfig(QStringLiteral("kdeglobals")))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(DEBOUNCE_MS);
    connect(&m_debounce, &QTimer::timeout, this, &AccentSync::evaluate);

    m_timeout.setSingleShot(true);
    m_timeout.setInterval(TIMEOUT_MS);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_proc) {
            qCWarning(KDOS_KDED) << "kdos theme" << m_running << "did not finish; stopping it";
            m_timedOut = true;
            m_proc->kill();
        }
    });

    m_watcher = KConfigWatcher::create(m_globals);
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this,
            [this](const KConfigGroup &group, const QByteArrayList &names) {
                if (group.name() == QLatin1String("General")
                    && names.contains(QByteArrayLiteral("ColorScheme"))) {
                    schedule();
                }
            });

    m_accent = readAccent(accentFile());

    /* Once at load: a login whose scheme was chosen while this was not
     * running — a first login, a scheme set from a terminal — is brought into
     * step now rather than at the next change. */
    schedule();
}

QString AccentSync::accent() const
{
    return m_accent;
}

QString AccentSync::state() const
{
    return m_state;
}

QString AccentSync::lastError() const
{
    return m_error;
}

void AccentSync::resync()
{
    m_reported.clear();
    m_force = true;
    schedule();
}

void AccentSync::schedule()
{
    m_debounce.start();
}

QString AccentSync::accentFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/kdos/accent");
}

QDateTime AccentSync::accentStamp()
{
    /* `kdos theme` writes the file beside and renames it over, so every
     * write is a new modification time, and a missing file is an invalid
     * one. */
    return QFileInfo(accentFile()).lastModified();
}

QString AccentSync::kdosProgram()
{
    /* kded inherits the session's PATH, which may not carry /usr/local/bin,
     * where kdos-tools installs the command. */
    QString path = QStandardPaths::findExecutable(QStringLiteral("kdos"));

    if (path.isEmpty()) {
        path = QStandardPaths::findExecutable(QStringLiteral("kdos"),
                                              {QStringLiteral("/usr/local/bin"),
                                               QStringLiteral("/usr/bin")});
    }
    return path;
}

void AccentSync::evaluate()
{
    m_globals->reparseConfiguration();

    const QString scheme = m_globals->group(QStringLiteral("General"))
                               .readEntry(QStringLiteral("ColorScheme"), QString());
    const QString accent = accentFromScheme(scheme);

    if (accent.isEmpty()
        || !isShippedScheme(scheme,
                            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation),
                            QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))) {
        return;
    }

    /* One run at a time. A different accent picked while one runs is run
     * after it, whatever the accent file says by then: the run in progress
     * writes that file first, so it can already name the new pick's accent
     * while the rest of the system is still going to the old one. */
    if (m_proc) {
        if (accent != m_running) {
            m_pending = true;
        }
        m_force = m_force && accent != m_running;
        return;
    }

    const bool force = m_force;
    const QString current = readAccent(accentFile());

    m_force = false;
    if (accent == current && !force) {
        /* A failed run leaves the file in step, because the file is the
         * first thing `kdos theme` writes; the failure stands until the file
         * is written again. In step any other way, whoever brought it there,
         * the state is idle. */
        if (accent == m_failedAccent && accentStamp() == m_failedStamp) {
            return;
        }
        m_failedAccent.clear();
        m_accent = current;
        m_state = QStringLiteral("idle");
        m_error.clear();
        return;
    }
    run(accent);
}

void AccentSync::run(const QString &accent)
{
    const QString program = kdosProgram();

    m_running = accent;
    if (program.isEmpty()) {
        conclude(false, i18nc("@info", "The kdos command is not installed."));
        return;
    }

    m_state = QStringLiteral("running");

    auto *proc = new QProcess(this);

    proc->setProgram(program);
    proc->setArguments({QStringLiteral("theme"), QStringLiteral("--from-plasma"), accent});
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->setStandardInputFile(QProcess::nullDevice());
    /* Each handler acts only for the run in progress: a run is concluded
     * once, and a signal from a process already concluded changes nothing. */
    connect(proc, &QProcess::finished, this, [this, proc](int exitCode, QProcess::ExitStatus status) {
        if (proc == m_proc) {
            finished(exitCode, status);
        }
    });
    /* A program that never starts emits no finished(), so its failure is
     * reported from here or the run would hold the slot until the timeout. */
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && proc == m_proc) {
            conclude(false,
                     i18nc("@info %1 is the system's reason", "The kdos command could not be started: %1",
                           proc->errorString()));
        }
    });
    m_proc = proc;
    m_timedOut = false;
    qCInfo(KDOS_KDED) << "the colour scheme names" << accent << "; running" << program
                      << proc->arguments();
    m_timeout.start();
    proc->start();
}

void AccentSync::finished(int exitCode, QProcess::ExitStatus status)
{
    if (status == QProcess::NormalExit && exitCode == 0) {
        conclude(true, QString());
        return;
    }

    QString message;

    if (m_timedOut) {
        message = i18nc("@info", "kdos theme did not finish within a minute and was stopped.");
    } else {
        /* The last thing the command said is the reason; `kdos` writes its
         * refusals as one line, and the last line of a run that failed half
         * way is the step that failed. */
        const QString output = m_proc ? QString::fromUtf8(m_proc->readAll()) : QString();
        const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

        message = lines.isEmpty() ? QString() : lines.last().trimmed();
        if (message.isEmpty()) {
            message = status == QProcess::NormalExit
                ? i18nc("@info", "kdos theme exited with status %1.", exitCode)
                : i18nc("@info", "kdos theme stopped before it finished.");
        }
    }
    conclude(false, message);
}

void AccentSync::conclude(bool ok, const QString &message)
{
    m_timeout.stop();
    if (m_proc) {
        m_proc->deleteLater();
        m_proc.clear();
    }

    const QString accent = m_running;

    m_running.clear();

    if (ok) {
        m_accent = accent;
        m_state = QStringLiteral("idle");
        m_error.clear();
        m_failedAccent.clear();
        qCInfo(KDOS_KDED) << "accent" << accent << "applied";
        Q_EMIT synced(accent);
    } else {
        m_state = QStringLiteral("failed");
        m_error = message.left(200);
        m_failedAccent = accent;
        m_failedStamp = accentStamp();
        qCWarning(KDOS_KDED) << "accent" << accent << "not applied:" << m_error;
        Q_EMIT failed(accent, m_error);
        notifyFailure(accent, m_error);
    }

    if (m_pending) {
        m_pending = false;
        schedule();
    }
}

void AccentSync::notifyFailure(const QString &accent, const QString &message)
{
    /* Once per accent and reason in a session: the same failure on every
     * later colour change is the same thing to fix, said again. */
    const QString key = accent + QLatin1Char('\n') + message;

    if (m_reported.contains(key)) {
        return;
    }
    m_reported.insert(key);

    auto *n = new KNotification(QStringLiteral("syncFailed"));

    n->setComponentName(QStringLiteral("kded_kdos"));
    n->setTitle(i18nc("@title", "Accent not applied everywhere"));
    n->setText(i18nc("@info %1 is an accent name, %2 the reason",
                     "The desktop is now in %1, but the rest of the system could not follow: %2",
                     accent.toHtmlEscaped(), message.toHtmlEscaped()));
    n->setIconName(QStringLiteral("preferences-desktop-color"));
    n->sendEvent();
}

}
