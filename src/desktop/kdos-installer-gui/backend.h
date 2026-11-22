/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   backend — the graphical installer's one door into kinstall
 *
 * Everything this program knows about the machine and everything it does to
 * it goes through `kinstall`, the same binary the terminal wizard is: the
 * pages are built from its dumps (`probe`, `look`, `apps`, `plan`, each as
 * JSON) and the install is `pkexec kinstall --config - --unattended --events
 * json`, the answers on its standard input. On the live medium the first
 * probe, Look Again's and the Summary's plan also run through pkexec, so
 * the probe activates LVM and the plan sees the volume groups that only
 * root can read. Nothing
 * here partitions, formats or writes a file of the target, so the two front
 * ends cannot disagree about what an install is. The one thing it changes on
 * the live medium itself is the session's keyboard layout, the Keyboard
 * page's choice, through KConfig and D-Bus.
 *
 * NO SHELL. Every program runs from an argument list through QProcess; a
 * disk path, a user name or a password is an argument or a line on a pipe,
 * never part of a command string.
 */

#pragma once

#include <QDBusMessage>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

#include "answers.h"
#include "eventstream.h"
#include "keyboardmodel.h"

namespace Kdos
{

class Backend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Kdos::Answers *answers READ answers CONSTANT)
    Q_PROPERTY(Kdos::EventStream *events READ events CONSTANT)
    Q_PROPERTY(Kdos::KeyboardModel *keyboards READ keyboards CONSTANT)

    Q_PROPERTY(QVariantMap probe READ probe NOTIFY probeChanged)
    Q_PROPERTY(QVariantMap look READ look NOTIFY lookChanged)
    Q_PROPERTY(QVariantMap apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(QVariantMap plan READ plan NOTIFY planChanged)
    /* A dump is being read; the page that asked waits for it. */
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    /* The last dump's failure, or "". */
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

    /* tzdata's zone1970.tab, one zone per region and no backward alias,
     * sorted, with UTC first: the list the terminal wizard offers. */
    Q_PROPERTY(QStringList timeZones READ timeZones CONSTANT)
    Q_PROPERTY(QString systemTimeZone READ systemTimeZone CONSTANT)

    Q_PROPERTY(bool partitionManagerAvailable READ partitionManagerAvailable CONSTANT)
    Q_PROPERTY(bool partitionManagerRunning READ partitionManagerRunning NOTIFY partitionManagerRunningChanged)

    Q_PROPERTY(bool installing READ installing NOTIFY installingChanged)
    /* kinstall's log, which the Done page offers to open. */
    Q_PROPERTY(QString logPath READ logPath CONSTANT)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    Answers *answers() const { return m_answers; }
    EventStream *events() const { return m_events; }
    KeyboardModel *keyboards() const { return m_keyboards; }

    QVariantMap probe() const { return m_probe; }
    QVariantMap look() const { return m_look; }
    QVariantMap apps() const { return m_apps; }
    QVariantMap plan() const { return m_plan; }
    bool busy() const { return !m_dumps.isEmpty(); }
    QString error() const { return m_error; }
    QStringList timeZones() const;
    QString systemTimeZone() const;
    bool partitionManagerAvailable() const;
    bool partitionManagerRunning() const { return m_partitionManager != nullptr; }
    bool installing() const { return m_install != nullptr; }
    QString logPath() const;

    /* The zones of a zone1970.tab, as timeZones lists them: column three,
     * sorted, UTC first; UTC alone when the file cannot be read. */
    static QStringList readZones(const QString &tabPath);
    /* Whether a mounts table (/proc/mounts) has an overlay at `/`: the live
     * medium, by the probe's own test, known before the first probe. */
    static bool liveRoot(const QString &mountsPath);
    /* The layout plan once a disk holding `partitions` partitions is
     * chosen: the erase plan for a disk with none, where the reuse plan has
     * nothing to use, else the plan as it was. */
    static QString diskPlan(const QString &plan, int partitions);
    /* kinstall's arguments for a `what` dump: the answers on standard input
     * when `withAnswers`, and the payload a root probe measured
     * (`payload_kb`, `appbox_kb` in `rootPayload`, empty when none has) for
     * every plan and for a probe not `asRoot`, so a dump as this user
     * decides on root's figures and a root probe measures again. */
    static QStringList dumpArgs(const QString &what, bool withAnswers, const QVariantMap &rootPayload, bool asRoot = false);
    Q_INVOKABLE QString planForDisk(const QString &plan, int partitions) const { return diskPlan(plan, partitions); }
    /* `probe` with root's copy figures (`copy_system_bytes`,
     * `copy_store_bytes` in `rootPayload`) where it has none, as a probe
     * handed root's payload has. */
    static QVariantMap withCopyBytes(const QVariantMap &probe, const QVariantMap &rootPayload);
    /* The install size a probe names: what the copy writes, the totals its
     * progress line counts to. The system's size alone, or with a box
     * store "<whole> — <system> system and <store> boxed-app store"; "—"
     * before a probe has measured. */
    static QString installSize(const QVariantMap &probe);
    Q_INVOKABLE QString installSizeText(const QVariantMap &probe) const { return installSize(probe); }
    /* A plan's swap and the kind it makes: none, a file in the root, a
     * partition, or a volume in the group (the A/B layout's swap, whatever
     * kind was asked for). */
    static QString swapText(const QVariantMap &plan);
    Q_INVOKABLE QString planSwapText(const QVariantMap &plan) const { return swapText(plan); }
    /* A probed disk (one of the probe's `disks`) as every page names it:
     * model, size and node, so the one disk among others of its size is
     * the one named. */
    static QString diskTitle(const QVariantMap &disk);
    /* A partition as a disk's contents list it: its type (EFI system, BIOS
     * boot, an LVM physical volume by its group, or the filesystem), its
     * label in quotes when it has one, and its size. */
    static QString partitionText(const QVariantMap &part);
    /* What erasing a disk destroys: its partitions, joined by ", ", or the
     * signature on the whole device of a disk with none; a disk the probe
     * marks `iso_image` (a written stick, its partitions all the image's
     * hybrid windows) by the image's label and size; "" for an empty
     * disk. A disk repartitioned over a stick keeps the iso9660 `fstype`
     * of its stale descriptor but not `iso_image`, and lists its
     * partitions. */
    static QString diskContents(const QVariantMap &disk);
    /* The line under a disk's row: "Holds: " and its contents, the
     * physical volume of a whole-disk PV, or "Empty". */
    static QString diskHolds(const QVariantMap &disk);
    /* diskTitle() and diskContents() of the probed disk at `path`: the
     * path itself and "" for a disk the last probe did not list. */
    Q_INVOKABLE QString diskName(const QString &path) const;
    Q_INVOKABLE QString diskErased(const QString &path) const;
    /* The partition at `path` on a disk of `probe` by its node and, in
     * brackets, partitionText(): "/dev/vda1 (EFI system 512 MiB)"; the
     * path alone for one the probe does not list, a logical volume among
     * them. partitionName() asks the last probe. */
    static QString partitionNamed(const QVariantMap &probe, const QString &path);
    Q_INVOKABLE QString partitionName(const QString &path) const { return partitionNamed(m_probe, path); }
    /* The ESP the reuse plan chooses unasked among `candidates` (the Disk
     * page's ESP list): the only one, when it is typed as an EFI system
     * partition; "" otherwise, as a lone FAT partition may hold data or
     * another system's boot files and formatting it is on by default. */
    static QString soleEsp(const QVariantList &candidates);
    Q_INVOKABLE QString onlyEsp(const QVariantList &candidates) const { return soleEsp(candidates); }
    Q_INVOKABLE QString holds(const QVariantMap &disk) const { return diskHolds(disk); }
    /* The layout `layout` (and `variant`, which may be empty) as the
     * session under `configHome` types: kxkbrc's [Layout] lists it first,
     * with `us` after it when it is not `us`. False, with nothing written,
     * for a name outside XKB's character set (letters, digits, `_+:.-`) or
     * a file that cannot be written. */
    static bool writeLiveLayout(const QString &configHome, const QString &layout, const QString &variant);
    /* KConfig's ConfigChanged notice for kxkbrc's [Layout] keys, the one
     * signal on which KWin loads the layout list again. */
    static QDBusMessage layoutNotice();
    /* On the live medium only: writeLiveLayout() on this session's
     * configuration, then layoutNotice() on the session bus, so KWin's
     * keymap, and fcitx5 decoding with it, is the chosen layout and every
     * field after the Keyboard page holds what the chosen keys type on the
     * installed machine. Anywhere else it does nothing: the layout is the
     * install's answer, not this session's setting. */
    Q_INVOKABLE void applyLiveLayout(const QString &layout, const QString &variant);

    /* What the machine is: its disks and partitions, its firmware, whether
     * this is the live medium. The first on the live medium runs as root,
     * as Look Again does, so the volume groups the live boot left inactive
     * are active before any page lists a volume; asked again while that
     * one runs, it waits for it and then runs once as this user. */
    Q_INVOKABLE void refreshProbe();
    /* The probe again, after a disk was attached. On the live medium it
     * runs as root through pkexec (the install's action, which the live
     * rule grants without a password), because only a root probe activates
     * the volume groups of a disk attached after boot. A refused or
     * dismissed request falls back to the probe as this user. */
    Q_INVOKABLE void lookAgain();
    /* The accents and desks the backend accepts. */
    Q_INVOKABLE void refreshLook();
    /* The application groups, ticked as the answers tick them. */
    Q_INVOKABLE void refreshApps();
    /* What these answers would do, and whether they are refused. On the
     * live medium it runs as root through pkexec, as Look Again does: an
     * inactive volume group named kdos on another disk, which the install
     * refuses, is seen only by root. A refused request falls back to the
     * plan as this user. */
    Q_INVOKABLE void refreshPlan();
    /* The plan for a page that asks after each change of an answer it
     * shows a refusal of, the Disk page's swap. As this user, deciding on
     * the root probe's payload as the Summary's plan does; on the live
     * medium with no root payload kept, as root through pkexec, like the
     * Summary's. */
    Q_INVOKABLE void checkPlan();

    /* Partition Manager, for a layout the guided plans do not make; the
     * disks are probed again when it closes. */
    Q_INVOKABLE void openPartitionManager();

    Q_INVOKABLE void startInstall();
    /* Restart through login1, which asks polkit like any other session's
     * request would. */
    Q_INVOKABLE void reboot();

    /* An accent's colours out of its KDOS<Label> colour scheme, for a
     * swatch: window, view, text, highlight, highlightedText, and accent,
     * the view's focus decoration. The accent is the colour itself; a dark
     * accent's highlight is that colour darkened until text reads on it.
     * Empty when the scheme is not installed. */
    Q_INVOKABLE QVariantMap accentColors(const QString &accent) const;
    /* The global theme's preview picture, as a file URL, or "". */
    Q_INVOKABLE QString accentPreview(const QString &lookAndFeel) const;
    /* A size in bytes, as a person reads it, with `precision` decimals. */
    Q_INVOKABLE QString formatSize(double bytes, int precision = 1) const;
    /* An accent's name as the Look page shows it: its colour scheme's name
     * without "KDOS ", or the id capitalised where no scheme is installed. */
    Q_INVOKABLE QString accentLabel(const QString &accent) const;
    /* A desk's title, as the Look page and the Summary name it. */
    Q_INVOKABLE QString deskTitle(const QString &desk) const;
    /* The time now in a zone, as hours and minutes. */
    Q_INVOKABLE QString localTime(const QString &zone) const;
    /* Whether this session may read the log; it is root's and `wheel`'s. */
    Q_INVOKABLE bool logReadable() const;
    /* The log in the session's text editor, raised over this window: KIO
     * opens it as plain text, so no application chooser is asked, and
     * hands the editor this window's activation token. */
    Q_INVOKABLE void openLog() const;

Q_SIGNALS:
    void probeChanged();
    void lookChanged();
    void appsChanged();
    void planChanged();
    void busyChanged();
    void errorChanged();
    void partitionManagerRunningChanged();
    /* Partition Manager ran and has closed; the probe that answers next is
     * the first look at the disks it left. Not sent when it failed to start. */
    void partitionManagerClosed();
    void installingChanged();
    void installFinished(bool ok);

private:
    using Done = std::function<void(const QVariantMap &)>;
    void runDump(const QString &what, bool withAnswers, Done done, bool asRoot = false);
    void runProbe(bool asRoot);
    void orphanRootDump(QProcess *p);
    void setError(const QString &e);
    QVariantMap probedDisk(const QString &path) const;

    Answers *m_answers;
    EventStream *m_events;
    KeyboardModel *m_keyboards;

    QVariantMap m_probe, m_look, m_apps, m_plan;
    /* payload_kb, appbox_kb and the copy figures from the last probe that
     * ran as root. */
    QVariantMap m_rootPayload;
    QString m_error;
    QHash<QString, QProcess *> m_dumps;
    QTimer m_probeTimer;
    QString m_probeKey;
    const bool m_live;
    bool m_probedAsRoot = false;
    /* A probe was asked for while a root probe ran. */
    bool m_probeAgain = false;
    QProcess *m_partitionManager = nullptr;
    QProcess *m_install = nullptr;
};

}
