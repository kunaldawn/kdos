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
Plan removePlan(const Entry &entry, const PlanContext &ctx);

// The packd request that a kdos-box or kdos-appbox step would otherwise need
// a wrapper for, as the daemon spells it.
QString packdRequest(const QString &verb, const QString &id);

}
