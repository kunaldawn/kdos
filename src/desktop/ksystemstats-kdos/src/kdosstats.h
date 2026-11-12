/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ksystemstats-kdos — what the sensors are read from
 *
 * Every reading the plugin publishes, as plain functions over a directory,
 * a file or a reply, so the QtTest suite can feed them fixtures and no test
 * needs a cgroup tree, kdos-energyd or a boot state. The plugin owns the
 * timing and the sensor objects; nothing here keeps state between calls
 * except what a caller passes back in.
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <sys/types.h>

namespace Kdos
{

/* ── boxes ─────────────────────────────────────────────────────────────── */

/* One cgroup a box runs in. A box has one scope while it runs; two only for
 * the moment a restarted box's old scope has not yet been removed, and the
 * plugin adds them up. */
struct BoxScope {
    QString box;
    QString path;
};

/* The box a scope is named for: `app-kdos-<box>-<n>.scope`, with the box's
 * dashes escaped as `\x2d`, the shape kdos-appbox gives it. An empty string
 * for any other name. */
QString kdosBoxFromScope(const QString &scopeName);

/* Every box scope this uid owns under ROOT (normally /sys/fs/cgroup), in the
 * two places kdos-appbox may put one: a login's delegated subtree,
 * `<session>/delegated/`, and the per-user service tree,
 * `user.slice/user-<uid>.slice/user@<uid>.service/app.slice/`. A scope
 * another uid owns is somebody else's box and is never listed. Sorted by box,
 * then path. */
QList<BoxScope> kdosFindBoxScopes(const QString &root, uid_t uid);

/* The counters of one cgroup, read whole. A cgroup's cpu.stat, memory.current,
 * io.stat and pids.current count every descendant, so podman's libpod-<id>
 * and conmon children below the scope are inside the totals without being
 * walked. A file the kernel does not offer, because its controller is not
 * enabled for the scope, leaves its field at -1. */
struct CgroupCounters {
    bool present = false;
    qint64 cpuUsec = -1;
    qint64 memoryBytes = -1;
    qint64 memoryMaxBytes = -1; /* 0 when the file says `max` */
    qint64 readBytes = -1;
    qint64 writeBytes = -1;
    qint64 processes = -1;
};

CgroupCounters kdosReadCgroup(const QString &dir);

/* Two scopes of one box as one: each field the sum of those that are known. */
CgroupCounters kdosAddCounters(const CgroupCounters &a, const CgroupCounters &b);

/* The boxes with no scope of their own, by their processes: every process
 * under PROCROOT (normally /proc) that UID owns, that is in a mount
 * namespace other than SELFPID's and whose environment carries
 * KDOS_BOX=<box> — the variable kdos-appbox sets in every box's container,
 * which podman passes to every exec. The namespace test keeps a host
 * process that only inherited the variable out. The box's figures are its
 * processes' sums: cpuUsec is utime, stime and the reaped children's cutime
 * and cstime, memoryBytes the proportional set size (smaps_rollup's Pss, the
 * resident set where that file cannot be read), readBytes and writeBytes
 * /proc/<pid>/io's read_bytes and write_bytes, and processes the count. Such
 * a box has no cgroup limit, so memoryMaxBytes is 0. A box named in SKIP is
 * counted by its scope and not here. */
QHash<QString, CgroupCounters> kdosBoxesByProcess(const QString &procRoot, const QString &selfPid, uid_t uid, const QStringList &skip = {});

/* What a box is called and drawn as in the menu: the launcher kdos-appbox
 * wrote for it, `X-KDOS-Box=<box>` in a .desktop file under one of DIRS.
 * The first launcher found wins, DIRS in order; a box with no launcher has
 * none and is shown by its own name. */
struct BoxLauncher {
    QString name;
    QString icon;
};

QHash<QString, BoxLauncher> kdosBoxLaunchers(const QStringList &dirs);

/* ── energy ────────────────────────────────────────────────────────────── */

/* One row of kdos-energyd's `report-json`. impact and gpuShare are fractions
 * of 1, as the daemon sends them. */
struct EnergyApp {
    QString id;
    QString name;
    QString app;
    QString box;
    double impact = 0;
    double cpuSeconds = 0;
    double gpuShare = -1; /* -1 when the daemon accounts no GPU */
};

struct EnergyReport {
    bool ok = false;
    QString error; /* the daemon's `err …` line, or why the reply is not a report */
    double windowSeconds = 0;
    int samples = 0;
    QString domains;
    double idleFloorWatts = 0;
    bool floorSettled = false;
    double attributable = 0;
    double shortLived = 0;
    int suspendedWindows = 0;
    QList<EnergyApp> apps;
};

EnergyReport kdosParseEnergyReport(const QByteArray &reply);

/* The sensor object id of an energy row: the app, `@` and the box for a boxed
 * one. Anything that is not a letter, digit, `.`, `_`, `-` or `@` becomes `_`,
 * so a name with a `/` in it cannot split the sensor path. */
QString kdosEnergyObjectId(const QString &app, const QString &box);

/* ── slots ─────────────────────────────────────────────────────────────── */

/* The A/B slots as `kdos-bootctl status --json` describes them, read from the
 * world-readable copy it keeps in /run/kdos/bootstate.json. */
struct SlotState {
    bool configured = false;
    QString running; /* "A" or "B", empty when the running slot is unknown */
    QString runningVersion;
    QString runningState;
    QString other;
    QString otherVersion;
    QString otherState;
    QString next;
    bool trial = false;
    int attempts = 0;
};

SlotState kdosParseSlotState(const QByteArray &json);

} // namespace Kdos
