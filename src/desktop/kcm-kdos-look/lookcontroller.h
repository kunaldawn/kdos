/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   lookcontroller — the accent everywhere, and Phosphor mode
 *
 * THREE ACCENTS ARE COMPARED, because three places hold one:
 *   Plasma's    kdeglobals [General] ColorScheme, set by the Global Theme
 *               and Colors pages as much as by `kdos theme`;
 *   KDOS's      `kdos theme list`'s current accent, which the terminals and
 *               the command-line tools wear;
 *   the machine's /etc/kdos/accent, which the boot menu, the splash and the
 *               login screen wear, written through kdos-powerd.
 * In step is all three the same. Plasma ahead of KDOS is an accent picked on
 * the Global Theme page that kded-kdos has not carried down yet; the machine
 * behind is a login screen still in the old colours. Either is repaired by
 * `kdos theme --from-plasma <Plasma's accent>`, the very command kded-kdos
 * runs, so Plasma's choice is the one kept and Plasma is not re-applied. A
 * scheme that is not KDOS's is a choice to leave KDOS out, and nothing is
 * offered.
 *
 * WHAT THE LINE NAMES IS WHAT EACH SURFACE WEARS. The login screen wears the
 * greeter set kdos-powerd copies, /usr/share/kdos/greeter/<accent>/, whose
 * kdeglobals names a light accent's dark partner, so that file, not the
 * machine's accent, is the login screen's colour. The boot menu is named only
 * where /boot/efi exists: a live medium has none, and kdos-bootctl cannot
 * recolour a menu it cannot write.
 *
 * kded-kdos IS THE OTHER RUNNER OF THAT COMMAND, and the page follows it over
 * io.kdos.Kded1 (kdedclient.h). While it runs, the page is busy and starts
 * nothing: two `kdos theme` runs at once would write the same files in an
 * order nobody chose. When its last run failed and the accents still
 * disagree, its reason is shown beside the repair.
 *
 * PHOSPHOR MODE IS `kdos phosphor`, read with `status --json` and switched
 * with `on`, `off`, `preset subtle|retro`, `terminus on|off` and `fullscreen
 * on|off`. The command owns the pairing of a retro accent with the effect and
 * what is restored; this page shows what it reports and never writes kwinrc
 * itself. A tuning made on the effect's own settings page (Fine-tune…) is
 * neither preset, and is shown as "custom" with neither radio checked.
 * Its options are offered only where the switch is: on software rendering
 * the mode can never come on, and an option recorded for it would wait for
 * an `on` that is refused. A refusal is said in the page's own words, from
 * the record's "refusal" ("session" or "gpu"); "reason", the command's own
 * sentence, is shown only for a refusal the page does not know.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <KConfigWatcher>

#include "accentmodel.h"
#include "kdedclient.h"
#include "kdoscommand.h"

class QDBusConnection;
class QFileSystemWatcher;
class QTimer;

namespace Kdos
{

class LookController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Kdos::AccentModel *accents READ accents CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyText READ busyText NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool commandFound READ commandFound CONSTANT)

    Q_PROPERTY(QString accent READ accent NOTIFY stateChanged)
    Q_PROPERTY(QString plasmaScheme READ plasmaScheme NOTIFY stateChanged)
    Q_PROPERTY(QString machineAccent READ machineAccent NOTIFY stateChanged)
    Q_PROPERTY(SyncState syncState READ syncState NOTIFY stateChanged)
    Q_PROPERTY(QString syncText READ syncText NOTIFY stateChanged)
    Q_PROPERTY(QString syncError READ syncError NOTIFY stateChanged)
    Q_PROPERTY(bool outOfStep READ outOfStep NOTIFY stateChanged)
    Q_PROPERTY(bool canSync READ canSync NOTIFY stateChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY busyChanged)

    Q_PROPERTY(bool phosphorKnown READ phosphorKnown NOTIFY phosphorChanged)
    Q_PROPERTY(bool phosphorOn READ phosphorOn NOTIFY phosphorChanged)
    Q_PROPERTY(bool phosphorSupported READ phosphorSupported NOTIFY phosphorChanged)
    /* The switch and every option can be used: the status was read, and the
     * mode is supported or already on (so it can be turned off). */
    Q_PROPERTY(bool phosphorUsable READ phosphorUsable NOTIFY phosphorChanged)
    Q_PROPERTY(QString phosphorReason READ phosphorReason NOTIFY phosphorChanged)
    Q_PROPERTY(QString preset READ preset NOTIFY phosphorChanged)
    Q_PROPERTY(bool terminus READ terminus NOTIFY phosphorChanged)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY phosphorChanged)
    Q_PROPERTY(bool fineTuneAvailable READ fineTuneAvailable CONSTANT)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY stateChanged)
    Q_PROPERTY(bool bootMenu READ bootMenu CONSTANT)

public:
    enum class SyncState {
        Unknown,
        InStep,
        PlasmaAhead,
        MachineBehind,
        NotKdos,
    };
    Q_ENUM(SyncState)

    struct Paths {
        QString program;
        QString machineAccent = QStringLiteral("/etc/kdos/accent");
        QString greeterDir = QStringLiteral("/usr/share/kdos/greeter");
        QString esp = QStringLiteral("/boot/efi");
    };

    /* kded-kdos is asked on the session bus. */
    explicit LookController(const Paths &paths, QObject *parent = nullptr);
    LookController(const Paths &paths, const QDBusConnection &session, QObject *parent = nullptr);

    AccentModel *accents() const;
    bool busy() const;
    QString busyText() const;
    QString errorMessage() const;
    bool commandFound() const;

    QString accent() const;
    QString plasmaScheme() const;
    QString machineAccent() const;
    SyncState syncState() const;
    QString syncText() const;
    /* Why kded-kdos's last run did not carry the accent down, while the
     * accents still disagree; empty otherwise. */
    QString syncError() const;
    /* Plasma ahead or the machine behind: something wears another accent. */
    bool outOfStep() const;
    bool canSync() const;
    /* kded-kdos is running `kdos theme --from-plasma` now. */
    bool syncing() const;

    bool phosphorKnown() const;
    bool phosphorOn() const;
    bool phosphorSupported() const;
    bool phosphorUsable() const;
    /* Why the mode cannot come on, translated; empty while it can. */
    QString phosphorReason() const;
    QString preset() const;
    bool terminus() const;
    /* The pass is drawn over fullscreen windows too; off keeps direct
     * scanout for games and video. */
    bool fullscreen() const;
    /* KWin's own settings page for the effect is installed. */
    bool fineTuneAvailable() const;
    bool reducedMotion() const;
    /* The machine has a boot menu an accent can recolour: Paths::esp is a
     * directory. */
    bool bootMenu() const;
    /* The accent the login screen wears for the machine's accent: its
     * greeter set's colour scheme, else the machine's accent itself. */
    QString loginAccent() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool applyAccent(const QString &accent);
    Q_INVOKABLE bool syncNow();
    Q_INVOKABLE bool setPhosphor(bool on);
    Q_INVOKABLE bool setPreset(const QString &preset);
    Q_INVOKABLE bool setTerminus(bool on);
    Q_INVOKABLE bool setFullscreen(bool on);
    /* The effect's own page (Strength, Scanlines, Curvature), in a dialog
     * of its own: KWin's kwin_kdos_crt_config. */
    Q_INVOKABLE void openFineTune();
    Q_INVOKABLE void dismissError();

Q_SIGNALS:
    void busyChanged();
    void errorMessageChanged();
    void stateChanged();
    void phosphorChanged();
    /* Each command's end, after its effects have been read back. */
    void commandDone(const QStringList &arguments, bool ok);

private:
    /* No action of the page's own is in flight, and kded-kdos runs none. */
    bool canAct() const;
    void act(const QStringList &arguments, const QString &busyText);
    void onFinished(const KdosCommand::Result &result);
    void readPlasma();
    void readMachine();
    void parsePhosphor(const QString &json);
    void setError(const QString &message);
    static QString failureText(const KdosCommand::Result &result);

    KdosCommand *m_command;
    KdedClient *m_kded;
    AccentModel *m_accents;
    Paths m_paths;
    KConfigWatcher::Ptr m_watcher;
    QFileSystemWatcher *m_files;
    QTimer *m_debounce;

    bool m_refreshQueued = false;
    /* An action is in flight from its click until the read-back after it;
     * a second click in that time is refused rather than queued. */
    bool m_pendingAction = false;
    QStringList m_actionArguments;
    bool m_actionOk = false;

    QString m_busyText;
    QString m_error;
    QString m_accent;
    QString m_plasmaScheme;
    QString m_machineAccent;
    /* The colour scheme the machine accent's greeter set names; empty when
     * the set or its key is missing. */
    QString m_loginScheme;
    bool m_reducedMotion = false;

    bool m_phosphorKnown = false;
    bool m_phosphorOn = false;
    bool m_phosphorSupported = false;
    QString m_phosphorReason;
    QString m_phosphorRefusal;
    QString m_preset = QStringLiteral("subtle");
    bool m_terminus = false;
    bool m_fullscreen = false;
};

} // namespace Kdos
