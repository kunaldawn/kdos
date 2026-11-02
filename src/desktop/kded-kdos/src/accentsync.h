/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — the accent, kept in step with Plasma
 *
 * An accent chosen inside Plasma — a KDOS global theme on the Global Theme
 * page, a KDOS colour scheme on the Colours page, the light/dark flip of
 * Quick Settings — changes the colour scheme and nothing below the desktop.
 * This watches kdeglobals `[General] ColorScheme` and, when it names a KDOS
 * scheme that differs from the accent `kdos theme` last wrote, runs
 * `kdos theme --from-plasma <accent>`, which carries the choice to the
 * terminal programs, the Qt 5 palette, the wallpaper, the greeter, the boot
 * menu and the console. `--from-plasma` skips the step that applies the
 * global theme, so the run this starts cannot start another.
 *
 * A scheme that is not KDOS's changes nothing: the accent stays what it was,
 * and the boot menu keeps its colours while the desktop wears Breeze.
 * ---------------------------------
 */

#pragma once

#include <KConfigWatcher>
#include <KSharedConfig>

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QTimer>

namespace Kdos
{

class AccentSync : public QObject
{
    Q_OBJECT

public:
    explicit AccentSync(QObject *parent = nullptr);

    /* The accent the system was last brought to, or the one the accent
     * file names. */
    QString accent() const;
    /* "idle", "running" or "failed". */
    QString state() const;
    QString lastError() const;

    /* Evaluate now, and run `kdos theme` even when the accent file already
     * names the scheme's accent: the command writes that file before its
     * other steps, so a run that failed half way leaves it in step, and
     * only a forced run retries what failed. */
    void resync();

Q_SIGNALS:
    void synced(const QString &accent);
    void failed(const QString &accent, const QString &message);

private:
    void schedule();
    void evaluate();
    void run(const QString &accent);
    void finished(int exitCode, QProcess::ExitStatus status);
    /* The end of a run, whichever way it ended: the state, the signal, the
     * notification on a failure, and the change that queued behind it. */
    void conclude(bool ok, const QString &message);
    void notifyFailure(const QString &accent, const QString &message);
    static QString accentFile();
    static QDateTime accentStamp();
    static QString kdosProgram();

    KSharedConfigPtr m_globals;
    KConfigWatcher::Ptr m_watcher;
    QTimer m_debounce;
    QTimer m_timeout;
    QPointer<QProcess> m_proc;
    bool m_pending = false;
    bool m_force = false;
    bool m_timedOut = false;
    /* The accent of the last failed run and the accent file's time when it
     * failed: the failure stands until a run succeeds or the file is
     * rewritten, which is `kdos theme` having been run by hand. */
    QString m_failedAccent;
    QDateTime m_failedStamp;
    QString m_running;
    QString m_accent;
    QString m_state = QStringLiteral("idle");
    QString m_error;
    QSet<QString> m_reported;
};

}
