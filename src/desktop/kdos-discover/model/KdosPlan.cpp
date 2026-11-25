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
#include <QLocale>

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

static Step preview(const QStringList &arguments)
{
    Step s;
    s.kind = Step::Kind::Preview;
    s.program = u"kdos"_s;
    s.arguments = arguments;
    return s;
}

// The names after this many are counted, not listed.
static constexpr int PreviewNames = 12;

QString installPreviewText(const InstallPreview &p)
{
    // Number and unit joined by a no-break space: the dialog wraps its
    // text, and a break between them leaves the unit alone on a line. The
    // dialog's label reads the text as Text.StyledText, which collapses a
    // raw U+00A0 with the other white space, so the text goes out
    // HTML-escaped and the no-break space as the entity &nbsp;.
    const QString size = QLocale().formattedDataSize(qint64(p.bytes)).replace(u' ', QChar::Nbsp);
    // `downloads` counts every prebuilt package and `bytes` only what is not
    // in the binhost cache yet, so a total of no bytes is a reinstall from
    // that cache: it is said to be downloaded already, never "0 bytes".
    // `downloads` also counts the restores of development files, which are
    // said apart from the prebuilt installs; the size stays the whole
    // download, since a restore downloads its package too.
    const int restores = qBound(0, p.restores, p.downloads);
    const int prebuilt = p.downloads - restores;
    QString said;
    if (p.builds == 0 && p.downloads == 0) {
        said = i18n("Everything it needs is installed already.");
    } else if (p.builds == 0) {
        QString lead;
        if (prebuilt == 0)
            lead = i18np("Restores the development files of %1 installed package",
                         "Restores the development files of %1 installed packages",
                         restores);
        else if (restores == 0)
            lead = i18np("Installs %1 package", "Installs %1 packages", prebuilt);
        else
            lead = i18ncp("@info %2 says how many packages are installed",
                          "%2 and restores the development files of %1 installed package",
                          "%2 and restores the development files of %1 installed packages",
                          restores,
                          i18np("Installs %1 package", "Installs %1 packages", prebuilt));
        said = p.bytes == 0 ? i18nc("@info %1 says what is installed", "%1, already downloaded.", lead)
                            : i18nc("@info %1 says what is installed, %2 is a size", "%1; downloads %2.", lead, size);
    } else {
        QString lead = i18np("Builds %1 package here", "Builds %1 packages here", p.builds);
        if (prebuilt > 0)
            lead = i18ncp("@info %2 says how many packages are built", "%2 and installs %1 prebuilt", "%2 and installs %1 prebuilt", prebuilt, lead);
        if (restores > 0)
            lead = i18ncp("@info %2 says what is built and installed",
                          "%2 and restores the development files of %1 installed package",
                          "%2 and restores the development files of %1 installed packages",
                          restores,
                          lead);
        if (p.downloads == 0)
            said = i18nc("@info %1 says what is built", "%1; building can take hours.", lead);
        else if (p.bytes == 0)
            said = i18nc("@info %1 says what is built and installed", "%1, already downloaded; building can take hours.", lead);
        else
            said = i18nc("@info %1 says what is built and installed, %2 is a size", "%1, downloading %2; building can take hours.", lead, size);
    }
    said = said.toHtmlEscaped().replace(QChar::Nbsp, u"&nbsp;"_s);
    if (!p.built.isEmpty()) {
        QStringList names;
        for (const QString &n : p.built.mid(0, PreviewNames))
            names << n.toHtmlEscaped();
        QString list = names.join(u", "_s);
        if (p.built.size() > PreviewNames)
            list = i18nc("@info %1 is a list of names", "%1 and %2 more", list, p.built.size() - PreviewNames);
        said += u"<br/>"_s + i18nc("@info %1 is a list of package names", "Built here: %1", list);
    }
    return said;
}

// What `kdos-box remove <box> --purge` takes and leaves, in the words of its
// own output: the container goes with the profile and every launcher
// exported from it; a pack box's directory (its writable layer, snapshots and
// private home) stays, and so does the home folder, which the box shares.
static Step askRemoveBox(const QString &box, const QString &base)
{
    Step s;
    s.kind = Step::Kind::Ask;
    s.question = i18nc("@title:window", "Remove the box %1?", box);
    const QString name = box.toHtmlEscaped();
    if (base.startsWith("pack:"_L1))
        s.questionText = i18n(
            "The box %1 is deleted with its profile and every launcher exported from it. Its writable layer, snapshots and private home stay in "
            "~/.local/share/kdos/boxes/%1, and so do the files in your home folder.",
            name);
    else
        s.questionText = i18n(
            "The box %1 is deleted with every package installed in it, its profile and every launcher exported from it. The files in your home folder "
            "stay.",
            name);
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
        plan.steps << preview({u"update"_s, u"install"_s, u"--dry-run"_s, u"--prebuilt"_s, e.id});
        plan.steps << run(u"pkexec"_s, {ctx.portsHelper, u"install"_s, e.id}, Meter::None, 1, true);
        return plan;

    case Lane::PortSource:
        // The builder and its dry run take the dependency route from
        // `build-deps =` in /etc/kdos/update.conf; the sources are fetched,
        // so it downloads.
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a port name.", e.id));
        plan.steps << preview({u"update"_s, u"install"_s, u"--dry-run"_s, u"--source"_s, e.id});
        plan.steps << run(u"pkexec"_s, {ctx.portsBuilder, u"install"_s, e.id}, Meter::None, 1, true);
        return plan;

    case Lane::Archive:
        if (!fileArgumentOk(e.path))
            return refuse(i18n("%1 is not a readable file.", e.path));
        if (e.members.isEmpty())
            return refuse(i18n("%1 is not a KDOS set: it names no application to import.", e.path));
        plan.steps << run(u"kdos-appbox"_s, {u"import"_s, e.path}, Meter::AppboxImport, qMax(1, int(e.members.size())));
        return plan;

    case Lane::PackFile:
        if (!fileArgumentOk(e.path))
            return refuse(i18n("%1 is not a readable file.", e.path));
        if (e.signature == "refused"_L1)
            return refuse(i18n("kdos-pack refused %1: it is not a KDOS pack, or its payload or its signature does not verify.", e.path));
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
            plan.steps << run(u"kdos-box"_s, {u"stop"_s, box->name}) << run(u"kdos-box"_s, {u"remove"_s, box->name, u"--purge"_s});
            if (stored)
                plan.steps << packd(u"remove"_s, e.id);
            plan.steps << launchers();
            return plan;
        }
        if (box != ctx.boxes.constEnd()) {
            plan.steps << run(u"kdos-box"_s, {u"stop"_s, box->name}) << run(u"kdos-appbox"_s, {u"uninstall"_s, e.id});
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
        const QString box = boxNameForDebian(pkg);
        if (ctx.boxes.contains(box))
            plan.steps << run(u"kdos-box"_s, {u"stop"_s, box});
        plan.steps << run(u"kdos-appbox"_s, {u"uninstall"_s, u"deb:"_s + pkg});
        return plan;
    }

    case Lane::Pack: {
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a pack id.", e.id));
        if (e.packOrigin != "store"_L1)
            return refuse(i18n("%1 is on the medium; nothing of it is installed.", e.name));
        if (e.kind == "app"_L1 && ctx.boxes.contains(boxNameForPack(e.id)))
            plan.steps << run(u"kdos-box"_s, {u"stop"_s, boxNameForPack(e.id)}) << run(u"kdos-box"_s, {u"remove"_s, boxNameForPack(e.id), u"--purge"_s});
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
        plan.steps << askRemoveBox(e.id, ctx.boxes.value(e.id).base.isEmpty() ? e.boxBase : ctx.boxes.value(e.id).base);
        plan.steps << run(u"kdos-box"_s, {u"stop"_s, e.id}) << run(u"kdos-box"_s, {u"remove"_s, e.id, u"--purge"_s});
        return plan;

    case Lane::PortSource:
        // An installed port is one package whichever twin's page asks: the
        // same removal as the binhost's twin. Not installed, nothing of it
        // is here to remove.
        if (e.state == EntryState::None)
            return refuse(i18n("Host software arrives and leaves with the system version; it is not removed one program at a time."));
        [[fallthrough]];
    case Lane::Port:
        // Only a port installed on request that nothing installed needs. The
        // dry run is a Preview step before pkexec, which needs no root: what
        // it prints becomes the question Discover asks, and `kdos update
        // remove` refuses there, with its reason and without the password,
        // when the machine changed since the list was read.
        if (!isSafeId(e.id))
            return refuse(i18n("“%1” is not a port name.", e.id));
        if (e.state == EntryState::None)
            return refuse(i18n("%1 is not installed.", e.name));
        if (!e.neededBy.isEmpty())
            return refuse(i18n("%1 is needed by %2, which is installed.", e.name, e.neededBy));
        if (!e.removable)
            return refuse(i18n("%1 is part of the KDOS system; it arrives and leaves with the system version.", e.name));
        plan.steps << preview({u"update"_s, u"remove"_s, u"--dry-run"_s, e.id});
        plan.steps << run(u"pkexec"_s, {ctx.portsHelper, u"remove"_s, e.id});
        return plan;

    case Lane::Archive:
    case Lane::PackFile:
        return refuse(i18n("A file is not installed; what it carries is listed under its own name."));
    }
    return refuse(i18n("Nothing removes this."));
}

}
