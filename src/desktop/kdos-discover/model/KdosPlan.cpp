/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — what an install or a removal runs, step by step
 * ---------------------------------
 */

#include "KdosPlan.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace Kdos
{

int Plan::units() const
{
    int n = 0;
    for (const Step &s : steps)
        n += qMax(1, s.units);
    return n;
}

QString packdRequest(const QString &verb, const QString &id)
{
    return verb + u' ' + id;
}

static Step run(const QString &program, const QStringList &arguments, Meter meter = Meter::None, int units = 1, bool downloads = false)
{
    Step s;
    s.program = program;
    s.arguments = arguments;
    s.meter = meter;
    s.units = units;
    s.downloads = downloads;
    return s;
}

static Step packd(const QString &verb, const QString &id)
{
    Step s;
    s.kind = Step::Kind::Packd;
    s.request = packdRequest(verb, id);
    return s;
}

// Every install and removal that changes what a box offers ends here, because
// a regeneration writes the whole launcher set from every box and every pack:
// one run is the whole clean-up.
static Step launchers()
{
    return run(u"kdos-appbox"_s, {u"genlaunchers"_s, u"--packs"_s, u"--user"_s});
}

static Plan refuse(const QString &why)
{
    Plan p;
    p.refusal = why;
    return p;
}

static QString mediumFile(const PlanContext &ctx, const QString &id)
{
    return QDir(ctx.mediumPacks).filePath(id + u".kpack"_s);
}

// A pack becomes usable by its kind: an application gets a box, a data pack is
// grafted where its program looks, a runtime or a base is only stored for the
// packs that require it.
static void useStoredPack(Plan &plan, const QString &id, const QString &kind)
{
    if (kind == "app"_L1) {
        plan.steps << run(u"kdos-box"_s, {u"create"_s, boxNameForPack(id), u"base=pack:"_s + id});
        plan.steps << launchers();
    } else if (kind == "data"_L1) {
        plan.steps << packd(u"graft"_s, id);
    }
}

static void importPackFile(Plan &plan, const QString &file, const QString &id, const QString &kind)
{
    if (kind == "app"_L1) {
        plan.steps << run(u"kdos-box"_s, {u"import"_s, file, u"as"_s, boxNameForPack(id)});
        plan.steps << launchers();
        return;
    }
    plan.steps << run(u"kdos-box"_s, {u"import"_s, file});
    if (kind == "data"_L1)
        plan.steps << packd(u"graft"_s, id);
}

static bool fileArgumentOk(const QString &path)
{
    const QFileInfo fi(path);
    return fi.isAbsolute() && fi.isFile();
}

Plan installPlan(const Entry &e, const PlanContext &ctx)
{
    Plan plan;

    switch (e.lane) {
    case Lane::Catalogue: {
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a catalogue id.", e.id));
        const auto pack = ctx.packs.constFind(e.id);
        if (pack != ctx.packs.constEnd() && pack->origin == "store"_L1) {
            useStoredPack(plan, e.id, pack->kind);
            if (plan.steps.isEmpty())
                return refuse(i18n("%1 is already in the pack store.", e.name));
            return plan;
        }
        if (pack != ctx.packs.constEnd() && pack->origin == "medium"_L1) {
            importPackFile(plan, mediumFile(ctx, e.id), e.id, pack->kind);
            return plan;
        }
        const int chain = ctx.catalogue ? ctx.catalogue->chainLength(e.id) : 0;
        if (chain == 0)
            return refuse(i18n("The catalogue cannot build %1: a row it is built on is missing.", e.name));
        plan.steps << run(u"kdos-appbox"_s, {u"install"_s, e.id}, Meter::AppboxInstall, chain + 1, true);
        return plan;
    }

    case Lane::Set: {
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a catalogue group.", e.id));
        if (e.members.isEmpty())
            return refuse(i18n("The set %1 has no members.", e.name));
        plan.steps << run(u"kdos-appbox"_s, {u"install"_s, e.id}, Meter::AppboxSet, int(e.members.size()), true);
        return plan;
    }

    case Lane::Debian: {
        const QString pkg = e.packages.value(0);
        if (!isDebianPackage(pkg))
            return refuse(i18n("“%1” is not a Debian package name.", pkg));
        // base, the application's own image, and the box over it.
        plan.steps << run(u"kdos-appbox"_s, {u"install"_s, u"deb:"_s + pkg}, Meter::AppboxInstall, 3, true);
        return plan;
    }

    case Lane::Pack: {
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a pack id.", e.id));
        if (e.packOrigin == "medium"_L1 || e.state == EntryState::Upgradeable) {
            const QString file = mediumFile(ctx, e.id);
            if (e.state == EntryState::Upgradeable) {
                // The box, when there is one, composes the new copy at its
                // next start; packd refuses the install while it runs.
                plan.steps << run(u"kdos-box"_s, {u"import"_s, file});
                return plan;
            }
            importPackFile(plan, file, e.id, e.kind);
            return plan;
        }
        useStoredPack(plan, e.id, e.kind);
        if (plan.steps.isEmpty())
            return refuse(i18n("%1 is already in the pack store.", e.name));
        return plan;
    }

    case Lane::Box:
        return refuse(i18n("A box is made by installing an application, or with “kdos-box create”."));

    case Lane::Port:
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a port name.", e.id));
        plan.steps << run(u"pkexec"_s, {ctx.portsHelper, u"install"_s, e.id}, Meter::None, 1, true);
        return plan;

    case Lane::Archive:
        if (!fileArgumentOk(e.path))
            return refuse(i18n("%1 is not a readable file.", e.path));
        plan.steps << run(u"kdos-appbox"_s, {u"import"_s, e.path}, Meter::AppboxImport, qMax(1, int(e.members.size())));
        return plan;

    case Lane::PackFile:
        if (!fileArgumentOk(e.path))
            return refuse(i18n("%1 is not a readable file.", e.path));
        if (!isSafeId(e.id))
            return refuse(i18n("%1 does not name a valid pack id.", e.path));
        importPackFile(plan, e.path, e.id, e.kind);
        return plan;
    }
    return refuse(i18n("Nothing installs this."));
}

Plan removePlan(const Entry &e, const PlanContext &ctx)
{
    Plan plan;

    switch (e.lane) {
    case Lane::Catalogue: {
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a catalogue id.", e.id));
        const auto box = ctx.boxes.constFind(e.box.isEmpty() ? e.id : e.box);
        const auto pack = ctx.packs.constFind(e.id);
        const bool stored = pack != ctx.packs.constEnd() && pack->origin == "store"_L1;
        if (box != ctx.boxes.constEnd() && box->base.startsWith("pack:"_L1)) {
            plan.steps << run(u"kdos-box"_s, {u"remove"_s, box->name});
            if (stored)
                plan.steps << packd(u"remove"_s, e.id);
            plan.steps << launchers();
            return plan;
        }
        if (box != ctx.boxes.constEnd()) {
            plan.steps << run(u"kdos-appbox"_s, {u"uninstall"_s, e.id});
            return plan;
        }
        if (stored && pack->kind == "data"_L1) {
            plan.steps << packd(u"ungraft"_s, e.id) << packd(u"remove"_s, e.id);
            return plan;
        }
        if (stored) {
            plan.steps << packd(u"remove"_s, e.id) << launchers();
            return plan;
        }
        return refuse(i18n("%1 is not installed.", e.name));
    }

    case Lane::Set:
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a catalogue group.", e.id));
        plan.steps << run(u"kdos-appbox"_s, {u"uninstall"_s, e.id}, Meter::None, int(qMax<qsizetype>(1, e.members.size())));
        return plan;

    case Lane::Debian: {
        const QString pkg = e.packages.value(0);
        if (!isDebianPackage(pkg))
            return refuse(i18n("“%1” is not a Debian package name.", pkg));
        plan.steps << run(u"kdos-appbox"_s, {u"uninstall"_s, u"deb:"_s + pkg});
        return plan;
    }

    case Lane::Pack: {
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a pack id.", e.id));
        if (e.packOrigin != "store"_L1)
            return refuse(i18n("%1 is on the medium; nothing of it is installed.", e.name));
        if (e.kind == "app"_L1 && ctx.boxes.contains(boxNameForPack(e.id)))
            plan.steps << run(u"kdos-box"_s, {u"remove"_s, boxNameForPack(e.id)});
        if (e.kind == "data"_L1)
            plan.steps << packd(u"ungraft"_s, e.id);
        plan.steps << packd(u"remove"_s, e.id);
        if (e.kind == "app"_L1)
            plan.steps << launchers();
        return plan;
    }

    case Lane::Box:
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a box name.", e.id));
        plan.steps << run(u"kdos-box"_s, {u"stop"_s, e.id}) << run(u"kdos-box"_s, {u"remove"_s, e.id});
        return plan;

    case Lane::Port:
        return refuse(i18n("Host software arrives and leaves with the system version; it is not removed one program at a time."));

    case Lane::Archive:
    case Lane::PackFile:
        return refuse(i18n("A file is not installed; what it carries is listed under its own name."));
    }
    return refuse(i18n("Nothing removes this."));
}

}
