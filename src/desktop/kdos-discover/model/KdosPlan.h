/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — what an install or a removal runs, step by step
 *
 * A PLAN IS A LIST OF ARGUMENT VECTORS AND PACKD REQUESTS, never a command
 * line. Every name in it came out of a tool's output or a file somebody handed
 * over, so each is checked against the id rule before it is placed in a step,
 * and a step is run with a program and its arguments, no shell between.
 *
 * A HOST PORT IS SHOWN BEFORE IT IS INSTALLED. Its plan opens with the dry
 * run of the same install, which needs no root: what it prints becomes the
 * question Discover asks (Transaction::proceedRequest), and only a yes
 * reaches pkexec and the password. A dry run that refuses ends the plan
 * there, with its reason, and nobody is asked for a password for an install
 * that would refuse.
 *
 * THE PLAN PREFERS A PACK TO A BUILD. A catalogue row whose pack is already in
 * the pack store, or on the medium, becomes a box over that pack in seconds
 * and with no network; only a row with no pack anywhere is built by the store
 * lane, which is minutes of apt against the Debian archive.
 * ---------------------------------
 */

#pragma once

#include "KdosEntry.h"
#include "KdosParse.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace Kdos
{

// How a step's output turns into progress.
enum class Meter {
    None, // done when it exits
    AppboxInstall, // one unit per "==> building" or "is already here" line
    AppboxSet, // one unit per "==> <id> installed" line
    AppboxImport, // one unit per "==> <id>" line
};

struct Step {
    enum class Kind {
        Run, // program + arguments
        Packd, // one request line to kdos-packd
        Preview, // program + arguments printing a dry run, asked about before the next step
    };
    Kind kind = Kind::Run;
    QString program;
    QStringList arguments;
    QString request;
    Meter meter = Meter::None;
    int units = 1; // this step's share of the transaction's progress
    bool downloads = false; // the step reaches the network
};

// What the backend knows about the machine when the plan is made.
struct PlanContext {
    const Catalogue *catalogue = nullptr;
    QHash<QString, PackRow> packs; // kdos-packd's list, by id, store copy preferred
    QHash<QString, BoxRow> boxes; // kdos-box list, by name
    QString mediumPacks = QStringLiteral("/mnt/iso/packs");
    QString portsHelper = QStringLiteral("/usr/libexec/kdos-discover/kdos-ports-helper");
    QString portsBuilder = QStringLiteral("/usr/libexec/kdos-discover/kdos-ports-builder");
};

struct Plan {
    QList<Step> steps;
    QString refusal; // why there is no plan, for the user; empty when there is one
    bool isValid() const
    {
        return refusal.isEmpty() && !steps.isEmpty();
    }
    int units() const;
};

Plan installPlan(const Entry &entry, const PlanContext &ctx);
// Every removal of a box stops it first: a box stays up after its
// application's last window closes until it stops itself, and `kdos-box
// remove` refuses a running container.
Plan removePlan(const Entry &entry, const PlanContext &ctx);

// The packd request that a kdos-box or kdos-appbox step would otherwise need
// a wrapper for, as the daemon spells it.
QString packdRequest(const QString &verb, const QString &id);

// The question a Preview step's dry run becomes: what is downloaded and how
// much, and what is built here with the warning that building takes long,
// then the packages built here by name. Rich text (Discover's dialog shows
// StyledText); every name in it is escaped.
QString installPreviewText(const InstallPreview &preview);

}
