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
 *                 beside the binhost's index as swcatalog/xml/kdos-binhost.xml,
 *                 and carries each component's icon file (embedIcon)
 *
 * KdosBackend lists host software from these and kpkg's database; nothing
 * else reads them.
 *
 *   kdos-discover-catalog --debian-exclude LIST --out FILE [--root DIR]
 *
 * writes a third, kdos-debian-exclude: one remove-component merge, priority
 * 1, for every desktop application of <root>/usr/share/swcatalog/yaml's
 * DEP-11 catalogues whose package LIST names (KdosBackend's debian-exclude,
 * one package a line, # for a comment). libappstream applies the merges
 * after every catalogue has loaded and drops each lower-priority component
 * of that id, so no AppStream pool on the machine has those applications:
 * KRunner's software results read the pool and nothing else, and would
 * otherwise offer an application the Debian lane refuses to build. An id
 * <root>/usr/share/swcatalog/xml/kdos-ports.xml also has is left out: its
 * merge would remove the host's own application with it. It fails with no
 * DEP-11 catalogue, and when LIST names nothing any catalogue has.
 *
 * THE OWNER COMES FROM KPKG'S MANIFESTS, never from a file name: every
 * <db>/<package> lists the paths the package installed, so a metainfo file or
 * a desktop entry belongs to exactly the package that says so.
 *
 * A PORT'S OWN METAINFO WINS. Where a package ships
 * usr/share/metainfo/<id>.metainfo.xml, that component is the record, given
 * the package name and, when it has none, the icon and the categories of the
 * desktop entry it launches: most metainfo files name no category, and
 * KdosBackend places a component in Discover's categories, All Applications
 * among them, and picks the icon of one whose own icon the machine lacks by
 * its categories. A component with no desktop-id <launchable> launches
 * usr/share/applications/<id> (<id>.desktop for an id without the suffix)
 * when the same package installs it, and is written with that launchable:
 * LibreOffice's legacy libreoffice-writer.desktop ids name no launchable,
 * and would otherwise be listed without an icon and a second time as their
 * desktop entry. A desktop entry no metainfo of the same package launches becomes a
 * component of its own, read by libappstream's desktop-entry reader; one that
 * is NoDisplay or Hidden, or not an application, describes nothing a person
 * installs and is skipped. A Terminal=true entry is written with the
 * ConsoleOnly category, which Discover's search ranks a terminal program by.
 *
 * A COMPONENT WITH NO NAME OR SUMMARY TAKES ONE FROM ITS DESKTOP ENTRY, or
 * from the recipe: Discover lists each by name with a summary line under it,
 * and an empty one is a blank row. The desktop entry is the bare entry itself,
 * or for metainfo the first entry the component launches. A component with
 * no untranslated name takes the entry's Name; one with no summary takes the
 * entry's Comment, else a GenericName that is not its name (compared without
 * case), else the port's `description =`. Both are written untranslated, with
 * the entry's \s, \n, \t and \\ escapes decoded.
 *
 * THE RECIPE FILLS IN THE LICENCE, THE HOMEPAGE AND THAT LAST SUMMARY, nothing
 * else. A component with no <project_license> takes its port's `license =` as
 * written, an SPDX expression, and one with no homepage <url> its
 * `homepage =`, all three read from the first
 * <tree>/<shelf>/<package>/kpkgbuild of <root>/ports/core, <root>/ports/src
 * (a machine's KDOS programs, by area) and <root>/kdos/src (the same recipes
 * where the 70_image chroot binds the repository, before 105_ports.sh has
 * copied them to /ports/src); the chroot binds the build's ports tree at
 * /ports. A package whose recipe is in none of them gets none of the three.
 * A value naming a `$` variable is left out: kpkgbuild is parsed,
 * never sourced, and an unexpanded name is no licence. A component read from
 * a bare desktop entry carries the custom value KDOS::BareEntry=1 and no
 * other component carries the key; in a catalogue with any such value
 * KdosBackend tells a suite's face (LibreOffice's start centre) from the
 * package's metainfo applications by the mark alone, and in one with none it
 * takes a desktop application with a .desktop id and no description for a
 * bare entry. A licence is no sign of metainfo, since every component takes
 * its recipe's. The <developer> comes only from a metainfo file the
 * package installs; a recipe names no author, so a component without one is
 * written without one and Discover says "Unknown author".
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
#include <QBuffer>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QXmlStreamWriter>

#include <algorithm>
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

// A key of the [Desktop Entry] group only: an action's icon is not the
// program's.
QString desktopValue(const QString &path, QLatin1StringView key)
{
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
        if (inEntry && line.startsWith(key) && line.mid(key.size()).trimmed().startsWith(u'='))
            return line.mid(line.indexOf(u'=') + 1).trimmed();
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

struct Recipe {
    QString license;
    QString homepage;
    QString description;
};

// <ports>/<shelf>/<name>/kpkgbuild for every port, read for its `license =`,
// `homepage =` and `description =` lines only.
QHash<QString, Recipe> readRecipes(const QDir &ports)
{
    QHash<QString, Recipe> recipes;
    const QStringList shelves = ports.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &shelf : shelves) {
        const QDir dir(ports.filePath(shelf));
        for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            QFile f(dir.filePath(name + u"/kpkgbuild"_s));
            if (!f.open(QIODevice::ReadOnly))
                continue;
            Recipe r;
            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine()).trimmed();
                const qsizetype eq = line.indexOf(u'=');
                if (line.startsWith(u'#') || eq < 0)
                    continue;
                const QString key = line.left(eq).trimmed();
                const QString value = line.mid(eq + 1).trimmed();
                if (value.isEmpty() || value.contains(u'$'))
                    continue;
                if (key == "license"_L1)
                    r.license = value;
                else if (key == "homepage"_L1)
                    r.homepage = value;
                else if (key == "description"_L1)
                    r.description = value;
            }
            if (!recipes.contains(name))
                recipes.insert(name, r);
        }
    }
    return recipes;
}

void fillFromRecipe(AppStream::Component &c, const Recipe &r)
{
    if (c.projectLicense().isEmpty() && !r.license.isEmpty())
        c.setProjectLicense(r.license);
    if (c.url(AppStream::Component::UrlKindHomepage).isEmpty() && !r.homepage.isEmpty())
        c.addUrl(AppStream::Component::UrlKindHomepage, r.homepage);
}

// A desktop entry's string value with its \s, \n, \t and \\ escapes decoded:
// written as found, "\s" would reach the catalogue as a backslash and an s.
// Any other escaped character is kept as it is.
QString desktopString(const QString &path, QLatin1StringView key)
{
    const QString raw = desktopValue(path, key);
    QString out;
    out.reserve(raw.size());
    for (qsizetype i = 0; i < raw.size(); ++i) {
        const QChar ch = raw.at(i);
        if (ch != u'\\' || i + 1 == raw.size()) {
            out += ch;
            continue;
        }
        const QChar next = raw.at(++i);
        if (next == u's')
            out += u' ';
        else if (next == u'n')
            out += u'\n';
        else if (next == u't')
            out += u'\t';
        else if (next == u'\\')
            out += u'\\';
        else {
            out += ch;
            out += next;
        }
    }
    return out;
}

// Discover lists every component by its name and a summary line under it,
// and an empty one leaves a blank row. Each is set untranslated ("C"), the
// value every locale falls back to.
void fillSummary(AppStream::Component &c, const QString &desktopPath, const Recipe &r)
{
    const QString name = desktopPath.isEmpty() ? QString() : desktopString(desktopPath, "Name"_L1);
    if (c.name().isEmpty() && !name.isEmpty())
        c.setName(name, u"C"_s);
    if (!c.summary().isEmpty())
        return;
    QString summary;
    if (!desktopPath.isEmpty()) {
        summary = desktopString(desktopPath, "Comment"_L1);
        if (summary.isEmpty()) {
            const QString generic = desktopString(desktopPath, "GenericName"_L1);
            const QString shown = c.name().isEmpty() ? name : c.name();
            if (generic.compare(shown, Qt::CaseInsensitive) != 0)
                summary = generic;
        }
    }
    if (summary.isEmpty())
        summary = r.description;
    if (!summary.isEmpty())
        c.setSummary(summary, u"C"_s);
}

bool hasIcon(const AppStream::Component &c)
{
    return !c.icons().isEmpty();
}

// The bytes a Discover row draws for icon `name` (a theme name, or an
// absolute path as a desktop entry's Icon= may give), from the root's hicolor
// theme (64 and 48 first, then scalable and the larger sizes, then the
// smaller) or /usr/share/pixmaps: a PNG at 64x64 or under is taken as it is, a
// larger one is scaled to 64x64, and an SVG is taken when it is 32 KiB or
// under. No result is over 32 KiB, so the catalogue stays a few megabytes.
QByteArray iconBytes(const QDir &root, const QString &name)
{
    constexpr qint64 cap = 32 * 1024;
    QStringList files;
    if (QDir::isAbsolutePath(name)) {
        files << root.filePath(name.mid(1));
    } else {
        for (const char *size : {"64x64", "48x48", "scalable", "128x128", "256x256", "512x512", "32x32", "24x24", "16x16"}) {
            const QString dir = root.filePath(u"usr/share/icons/hicolor/%1/apps/"_s.arg(QLatin1String(size)));
            files << dir + name + (qstrcmp(size, "scalable") == 0 ? u".svg"_s : u".png"_s);
        }
        files << root.filePath(u"usr/share/pixmaps/"_s + name + u".png"_s) << root.filePath(u"usr/share/pixmaps/"_s + name + u".svg"_s);
    }
    for (const QString &path : std::as_const(files)) {
        const QFileInfo fi(path);
        if (!fi.isFile())
            continue;
        if (fi.suffix() == "svg"_L1) {
            QFile f(path);
            if (fi.size() <= cap && f.open(QIODevice::ReadOnly))
                return f.readAll();
            continue;
        }
        QImage img(path);
        if (img.isNull())
            continue;
        if (img.width() <= 64 && img.height() <= 64 && fi.size() <= cap) {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly))
                return f.readAll();
            continue;
        }
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buf, "PNG");
        if (!png.isEmpty() && png.size() <= cap)
            return png;
    }
    return {};
}

// A binary host's catalogue describes packages the machine reading it has
// not installed, so the icon its stock name names is not in that machine's
// theme. The icon file goes into the catalogue itself, base64 in the custom
// value KDOS::IconData, which KdosBackend writes to its icon cache: the
// catalogue is signed and fetched already, and nothing else need be. The
// component's stock and local icons are tried first, then the Icon= of the
// desktop entry it launches, which is all an application whose metainfo
// names only a remote icon, or none, has.
void embedIcon(AppStream::Component &c, const QDir &root, const QString &desktopFile)
{
    QStringList names;
    const auto icons = c.icons();
    for (const AppStream::Icon &icon : icons) {
        if (icon.kind() == AppStream::Icon::KindStock)
            names << icon.name();
        else if (icon.kind() == AppStream::Icon::KindLocal && icon.url().isLocalFile())
            names << icon.url().toLocalFile();
    }
    if (!desktopFile.isEmpty()) {
        const QString entry = desktopValue(desktopFile, "Icon"_L1);
        if (!entry.isEmpty() && !names.contains(entry))
            names << entry;
    }
    for (const QString &name : std::as_const(names)) {
        const QByteArray bytes = iconBytes(root, name);
        if (!bytes.isEmpty()) {
            c.insertCustomValue(u"KDOS::IconData"_s, QString::fromLatin1(bytes.toBase64()));
            return;
        }
    }
}

// Replaces `out` in one rename(2), so a reader never sees half a catalogue.
bool replaceWith(const QString &tmp, const QString &out)
{
    if (std::rename(QFile::encodeName(tmp).constData(), QFile::encodeName(out).constData()) != 0) {
        std::fprintf(stderr, "kdos-discover-catalog: cannot rename %s to %s\n", qPrintable(tmp), qPrintable(out));
        QFile::remove(tmp);
        return false;
    }
    return true;
}

int writeDebianExclude(const QDir &root, const QString &listPath, const QString &out)
{
    QSet<QString> excluded;
    QFile list(listPath);
    if (!list.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "kdos-discover-catalog: cannot read %s\n", qPrintable(listPath));
        return 1;
    }
    for (const QByteArray &raw : list.readAll().split('\n')) {
        const int hash = raw.indexOf('#');
        const QString line = QString::fromUtf8(hash < 0 ? raw : raw.left(hash)).trimmed();
        if (!line.isEmpty())
            excluded.insert(line);
    }

    // A merge removes every component of its id, whatever catalogue holds
    // it, so an id the host's own catalogue has (org.kde.discover, gparted)
    // is never written: its merge would take the host application out too.
    QSet<QString> hostIds;
    const QString ports = root.filePath(u"usr/share/swcatalog/xml/kdos-ports.xml"_s);
    if (QFileInfo::exists(ports)) {
        AppStream::Metadata md;
        md.setFormatStyle(AppStream::Metadata::FormatStyleCatalog);
        if (md.parseFile(ports, AppStream::Metadata::FormatKindXml) != AppStream::Metadata::MetadataErrorNoError) {
            std::fprintf(stderr, "kdos-discover-catalog: %s: %s\n", qPrintable(ports), qPrintable(md.lastError()));
            return 1;
        }
        for (const AppStream::Component &c : md.components())
            hostIds.insert(c.id());
    }

    const QDir yaml(root.filePath(u"usr/share/swcatalog/yaml"_s));
    const QStringList files = yaml.entryList({u"*.yml"_s, u"*.yml.gz"_s}, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        std::fprintf(stderr, "kdos-discover-catalog: no DEP-11 catalogue in %s\n", qPrintable(yaml.path()));
        return 1;
    }
    QStringList ids;
    for (const QString &name : files) {
        AppStream::Metadata md;
        md.setFormatStyle(AppStream::Metadata::FormatStyleCatalog);
        if (md.parseFile(yaml.filePath(name), AppStream::Metadata::FormatKindYaml) != AppStream::Metadata::MetadataErrorNoError) {
            std::fprintf(stderr, "kdos-discover-catalog: %s: %s\n", qPrintable(name), qPrintable(md.lastError()));
            return 1;
        }
        for (const AppStream::Component &c : md.components()) {
            if (c.kind() == AppStream::Component::KindDesktopApp && excluded.contains(c.packageNames().value(0)) && !hostIds.contains(c.id())
                && !ids.contains(c.id()))
                ids << c.id();
        }
    }
    if (ids.isEmpty()) {
        std::fprintf(stderr, "kdos-discover-catalog: %s names no application of %s\n", qPrintable(listPath), qPrintable(yaml.path()));
        return 1;
    }
    ids.sort();

    QDir().mkpath(QFileInfo(out).absolutePath());
    const QString tmp = out + u".tmp"_s;
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::fprintf(stderr, "kdos-discover-catalog: cannot write %s\n", qPrintable(tmp));
        return 1;
    }
    QXmlStreamWriter xml(&f);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(u"components"_s);
    xml.writeAttribute(u"version"_s, u"1.0"_s);
    xml.writeAttribute(u"origin"_s, u"kdos-debian-exclude"_s);
    for (const QString &id : std::as_const(ids)) {
        xml.writeStartElement(u"component"_s);
        xml.writeAttribute(u"merge"_s, u"remove-component"_s);
        xml.writeAttribute(u"priority"_s, u"1"_s);
        xml.writeTextElement(u"id"_s, id);
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    if (!f.flush() || xml.hasError()) {
        std::fprintf(stderr, "kdos-discover-catalog: cannot write %s\n", qPrintable(tmp));
        f.close();
        QFile::remove(tmp);
        return 1;
    }
    f.close();
    if (!replaceWith(tmp, out))
        return 1;
    std::printf("%s: %lld applications removed from the pool\n", qPrintable(out), qlonglong(ids.size()));
    return 0;
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
    const QCommandLineOption excludeOpt(u"debian-exclude"_s,
                                        u"Write the catalogue that removes from the AppStream pool the Debian applications whose package this list names."_s,
                                        u"list"_s);
    parser.addOptions({rootOpt, outOpt, originOpt, packageOpt, excludeOpt});
    parser.process(app);

    if (parser.isSet(excludeOpt)) {
        const QDir root(parser.value(rootOpt));
        const QString out = parser.isSet(outOpt) ? parser.value(outOpt) : root.filePath(u"usr/share/swcatalog/xml/kdos-debian-exclude.xml"_s);
        return writeDebianExclude(root, parser.value(excludeOpt), out);
    }

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

    QHash<QString, Recipe> recipes;
    for (const QString &tree : {u"ports/core"_s, u"ports/src"_s, u"kdos/src"_s}) {
        const QHash<QString, Recipe> more = readRecipes(QDir(root.filePath(tree)));
        for (auto it = more.cbegin(); it != more.cend(); ++it) {
            if (!recipes.contains(it.key()))
                recipes.insert(it.key(), it.value());
        }
    }

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
        const Recipe recipe = recipes.value(pkg);
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
            fillFromRecipe(c, recipe);
            // A metainfo with no desktop entry, or naming only entries the
            // package does not install (Audacity's audacity.desktop beside
            // the org.audacityteam.Audacity.desktop it installs), launches
            // the entry its id implies when the package has that one. The
            // implied entry is added to the component's own launchable, so
            // the entry it installs is a launched one and gets no second
            // component of the same id below.
            QStringList entries = c.launchable(AppStream::Launchable::KindDesktopId).entries();
            const bool anyOwned = std::any_of(entries.cbegin(), entries.cend(), [&](const QString &e) {
                return owned.desktop.contains(u"usr/share/applications/"_s + e);
            });
            if (!anyOwned) {
                // The entry the id implies, or one the metainfo names, in the
                // case the package spells it (Stellarium's metainfo names
                // org.stellarium.Stellarium.desktop; it installs
                // org.stellarium.stellarium.desktop).
                QStringList wanted = entries;
                wanted << (c.id().endsWith(".desktop"_L1) ? c.id() : c.id() + u".desktop"_s);
                // A package of one metainfo and one desktop entry launches
                // that entry, whatever it is called (Viking's metainfo names
                // org.viking.Viking.desktop; it installs viking.desktop).
                QString implied;
                QStringList apps;
                for (const QString &rel : std::as_const(owned.desktop)) {
                    const QString file = QFileInfo(rel).fileName();
                    if (!rel.startsWith("usr/share/applications/"_L1)) {
                        continue;
                    }
                    apps << file;
                    if (implied.isEmpty() && wanted.contains(file, Qt::CaseInsensitive)) {
                        implied = file;
                    }
                }
                if (implied.isEmpty() && apps.size() == 1 && owned.metainfo.size() == 1) {
                    implied = apps.first();
                }
                if (!implied.isEmpty()) {
                    if (entries.isEmpty()) {
                        AppStream::Launchable made;
                        made.setKind(AppStream::Launchable::KindDesktopId);
                        made.addEntry(implied);
                        c.addLaunchable(made);
                    } else {
                        c.launchable(AppStream::Launchable::KindDesktopId).addEntry(implied);
                    }
                    entries << implied;
                }
            }
            QString firstLaunched;
            for (const QString &entry : std::as_const(entries)) {
                const QString desktopRel = u"usr/share/applications/"_s + entry;
                if (owned.desktop.contains(desktopRel)) {
                    launched.insert(desktopRel);
                    if (firstLaunched.isEmpty())
                        firstLaunched = root.filePath(desktopRel);
                    if (!hasIcon(c)) {
                        const QString icon = desktopValue(root.filePath(desktopRel), "Icon"_L1);
                        if (!icon.isEmpty())
                            c.addIcon(iconFor(icon));
                    }
                    if (c.categories().isEmpty()) {
                        const QStringList categories = desktopValue(root.filePath(desktopRel), "Categories"_L1).split(u';', Qt::SkipEmptyParts);
                        for (const QString &category : categories)
                            c.addCategory(category.trimmed());
                    }
                    if (desktopValue(root.filePath(desktopRel), "Terminal"_L1).compare("true"_L1, Qt::CaseInsensitive) == 0 && !c.categories().contains(u"ConsoleOnly"_s))
                        c.addCategory(u"ConsoleOnly"_s);
                }
            }
            fillSummary(c, firstLaunched, recipe);
            // An entry another package installs (flare-game's metainfo
            // launches flare-engine's flare.desktop) still names the icon.
            QString iconEntry = firstLaunched;
            for (const QString &entry : std::as_const(entries)) {
                if (!iconEntry.isEmpty())
                    break;
                if (QFileInfo::exists(root.filePath(u"usr/share/applications/"_s + entry)))
                    iconEntry = root.filePath(u"usr/share/applications/"_s + entry);
            }
            if (origin == "kdos-binhost"_L1)
                embedIcon(c, root, iconEntry);
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
            if (desktopValue(root.filePath(rel), "Terminal"_L1).compare("true"_L1, Qt::CaseInsensitive) == 0 && !c.categories().contains(u"ConsoleOnly"_s))
                c.addCategory(u"ConsoleOnly"_s);
            c.setPackageNames({pkg});
            c.insertCustomValue(u"KDOS::Version"_s, owned.version);
            c.insertCustomValue(u"KDOS::BareEntry"_s, u"1"_s);
            fillFromRecipe(c, recipe);
            fillSummary(c, root.filePath(rel), recipe);
            if (origin == "kdos-binhost"_L1)
                embedIcon(c, root, root.filePath(rel));
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
    if (!replaceWith(tmp, out))
        return 1;
    std::printf("%s: %d components from %lld packages, %d skipped\n", qPrintable(out), components, qlonglong(packages.size()), skipped);
    return 0;
}
