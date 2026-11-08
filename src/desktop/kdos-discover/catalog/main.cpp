/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover-catalog — the AppStream catalogue of a root's host ports
 *
 *   kdos-discover-catalog [--root DIR] [--out FILE] [--origin NAME]
 *                         [--package NAME]...
 *
 * One component per application a port installs, its <pkgname> the kpkg
 * package that owns it and its custom KDOS::Version that package's
 * `version-release`. Two catalogues are written with it:
 *
 *   kdos-ports    what a root has installed, the default: origin kdos-ports,
 *                 <root>/usr/share/swcatalog/xml/kdos-ports.xml, written over
 *                 the finished image and again after every install
 *   kdos-binhost  what a binary host can add: --origin kdos-binhost over the
 *                 build tree, which has every package installed, with
 *                 --package naming the packages that binhost holds; it goes
 *                 beside the binhost's index as swcatalog/xml/kdos-binhost.xml
 *
 * KdosBackend lists host software from these and kpkg's database; nothing
 * else reads them.
 *
 * THE OWNER COMES FROM KPKG'S MANIFESTS, never from a file name: every
 * <db>/<package> lists the paths the package installed, so a metainfo file or
 * a desktop entry belongs to exactly the package that says so.
 *
 * A PORT'S OWN METAINFO WINS. Where a package ships
 * usr/share/metainfo/<id>.metainfo.xml, that component is the record, given
 * the package name and, when it has none, the icon of the desktop entry it
 * launches. A desktop entry no metainfo of the same package launches becomes a
 * component of its own, read by libappstream's desktop-entry reader; one that
 * is NoDisplay or Hidden, or not an application, describes nothing a person
 * installs and is skipped.
 *
 * THE WHOLE FILE IS WRITTEN EVERY TIME, from what is on disk, to a temporary
 * name renamed into place, so a reader never sees half a catalogue and a
 * removed port leaves no component behind.
 * ---------------------------------
 */

#include <AppStreamQt/component.h>
#include <AppStreamQt/icon.h>
#include <AppStreamQt/launchable.h>
#include <AppStreamQt/metadata.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <cstdio>

using namespace Qt::StringLiterals;

namespace
{

struct Owned {
    QString version; // version-release
    QStringList metainfo; // root-relative paths
    QStringList desktop;
};

// <db>/<package>: a "version release" line, then one "./path" per line.
Owned ownedBy(const QString &manifest)
{
    Owned o;
    QFile f(manifest);
    if (!f.open(QIODevice::ReadOnly))
        return o;
    const QStringList first = QString::fromUtf8(f.readLine()).simplified().split(u' ', Qt::SkipEmptyParts);
    o.version = first.join(u'-');
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith("./"_L1))
            line = line.mid(2);
        if (line.startsWith("usr/share/metainfo/"_L1) && (line.endsWith(".metainfo.xml"_L1) || line.endsWith(".appdata.xml"_L1)))
            o.metainfo << line;
        else if (line.startsWith("usr/share/applications/"_L1) && line.endsWith(".desktop"_L1) && !line.mid(23).contains(u'/'))
            o.desktop << line;
    }
    return o;
}

QString desktopIcon(const QString &path)
{
    // The [Desktop Entry] group only: an action's icon is not the program's.
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    bool inEntry = false;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith(u'[')) {
            inEntry = line == "[Desktop Entry]"_L1;
            continue;
        }
        if (inEntry && line.startsWith("Icon="_L1))
            return line.mid(5).trimmed();
    }
    return {};
}

bool desktopIsOffered(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    bool inEntry = false, application = false;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith(u'[')) {
            inEntry = line == "[Desktop Entry]"_L1;
            continue;
        }
        if (!inEntry)
            continue;
        if (line == "NoDisplay=true"_L1 || line == "Hidden=true"_L1)
            return false;
        if (line == "Type=Application"_L1)
            application = true;
    }
    return application;
}

AppStream::Icon iconFor(const QString &value)
{
    AppStream::Icon icon;
    if (QDir::isAbsolutePath(value)) {
        icon.setKind(AppStream::Icon::KindLocal);
        icon.setUrl(QUrl::fromLocalFile(value));
    } else {
        icon.setKind(AppStream::Icon::KindStock);
        icon.setName(value);
    }
    return icon;
}

bool hasIcon(const AppStream::Component &c)
{
    return !c.icons().isEmpty();
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"kdos-discover-catalog"_s);

    QCommandLineParser parser;
    parser.setApplicationDescription(u"Write the AppStream catalogue of the applications a root's kpkg packages install."_s);
    parser.addHelpOption();
    const QCommandLineOption rootOpt(u"root"_s, u"The root whose kpkg database is read (default /)."_s, u"dir"_s, u"/"_s);
    const QCommandLineOption outOpt(u"out"_s, u"The catalogue to write (default <root>/usr/share/swcatalog/xml/<origin>.xml)."_s, u"file"_s);
    const QCommandLineOption originOpt(u"origin"_s, u"The catalogue's origin: kdos-ports (default) or kdos-binhost."_s, u"name"_s, u"kdos-ports"_s);
    const QCommandLineOption packageOpt(u"package"_s, u"Describe only this package; repeat for more."_s, u"name"_s);
    parser.addOptions({rootOpt, outOpt, originOpt, packageOpt});
    parser.process(app);

    const QString origin = parser.value(originOpt);
    if (origin != "kdos-ports"_L1 && origin != "kdos-binhost"_L1) {
        std::fprintf(stderr, "kdos-discover-catalog: --origin is kdos-ports or kdos-binhost, not %s\n", qPrintable(origin));
        return 2;
    }
    const QStringList wanted = parser.values(packageOpt);
    const QDir root(parser.value(rootOpt));
    const QString dbDir = root.filePath(u"var/lib/kpkg/db"_s);
    const QString out = parser.isSet(outOpt) ? parser.value(outOpt) : root.filePath(u"usr/share/swcatalog/xml/"_s + origin + u".xml"_s);

    QDir db(dbDir);
    if (!db.exists()) {
        std::fprintf(stderr, "kdos-discover-catalog: no package database at %s\n", qPrintable(dbDir));
        return 2;
    }

    AppStream::Metadata catalog;
    catalog.setFormatStyle(AppStream::Metadata::FormatStyleCatalog);
    catalog.setOrigin(origin);
    catalog.setWriteHeader(true);

    int components = 0, skipped = 0;
    const QStringList packages = db.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    // A package named and not installed is a binhost described wrong: its
    // catalogue would offer nothing for a package it holds.
    for (const QString &pkg : wanted) {
        if (!packages.contains(pkg)) {
            std::fprintf(stderr, "kdos-discover-catalog: %s is not installed in %s\n", qPrintable(pkg), qPrintable(root.absolutePath()));
            return 1;
        }
    }
    for (const QString &pkg : packages) {
        if (pkg.startsWith(u'.') || (!wanted.isEmpty() && !wanted.contains(pkg)))
            continue;
        const Owned owned = ownedBy(db.filePath(pkg));
        QSet<QString> launched;

        for (const QString &rel : owned.metainfo) {
            AppStream::Metadata md;
            if (md.parseFile(root.filePath(rel), AppStream::Metadata::FormatKindXml) != AppStream::Metadata::MetadataErrorNoError) {
                std::fprintf(stderr, "kdos-discover-catalog: %s: %s: %s\n", qPrintable(pkg), qPrintable(rel), qPrintable(md.lastError()));
                skipped++;
                continue;
            }
            AppStream::Component c = md.component();
            if (c.id().isEmpty()) {
                skipped++;
                continue;
            }
            c.setPackageNames({pkg});
            c.insertCustomValue(u"KDOS::Version"_s, owned.version);
            const AppStream::Launchable l = c.launchable(AppStream::Launchable::KindDesktopId);
            for (const QString &entry : l.entries()) {
                const QString desktopRel = u"usr/share/applications/"_s + entry;
                if (owned.desktop.contains(desktopRel)) {
                    launched.insert(desktopRel);
                    if (!hasIcon(c)) {
                        const QString icon = desktopIcon(root.filePath(desktopRel));
                        if (!icon.isEmpty())
                            c.addIcon(iconFor(icon));
                    }
                }
            }
            catalog.addComponent(c);
            components++;
        }

        for (const QString &rel : owned.desktop) {
            if (launched.contains(rel) || !desktopIsOffered(root.filePath(rel)))
                continue;
            QFile f(root.filePath(rel));
            if (!f.open(QIODevice::ReadOnly))
                continue;
            const QString cid = QFileInfo(rel).fileName();
            AppStream::Metadata md;
            if (md.parseDesktopData(cid, QString::fromUtf8(f.readAll())) != AppStream::Metadata::MetadataErrorNoError) {
                skipped++;
                continue;
            }
            AppStream::Component c = md.component();
            if (c.id().isEmpty() || c.name().isEmpty()) {
                skipped++;
                continue;
            }
            c.setPackageNames({pkg});
            c.insertCustomValue(u"KDOS::Version"_s, owned.version);
            catalog.addComponent(c);
            components++;
        }
    }

    QDir().mkpath(QFileInfo(out).absolutePath());
    const QString tmp = out + u".tmp"_s;
    QFile::remove(tmp);
    if (catalog.saveCatalog(tmp, AppStream::Metadata::FormatKindXml) != AppStream::Metadata::MetadataErrorNoError) {
        std::fprintf(stderr, "kdos-discover-catalog: cannot write %s: %s\n", qPrintable(tmp), qPrintable(catalog.lastError()));
        QFile::remove(tmp);
        return 1;
    }
    // rename(2) replaces the old catalogue in one step.
    if (std::rename(QFile::encodeName(tmp).constData(), QFile::encodeName(out).constData()) != 0) {
        std::fprintf(stderr, "kdos-discover-catalog: cannot rename %s to %s\n", qPrintable(tmp), qPrintable(out));
        return 1;
    }
    std::printf("%s: %d components from %lld packages, %d skipped\n", qPrintable(out), components, qlonglong(packages.size()), skipped);
    return 0;
}
