/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   library — what the offline library holds, shelf by shelf
 */

#include "library.h"

#include <KLocalizedString>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <QXmlStreamReader>

#include <algorithm>

namespace Kdos
{

namespace
{
const char *Domain = "kdos-kio-library";

/* A book or docset directory is summed file by file; one this large is not
 * a book, and its size is reported as what was counted. */
constexpr int MaxSizedEntries = 200000;
/* A library.xml or an Info.plist larger than this is not one. */
constexpr qint64 MaxMetadataFile = 16 * 1024 * 1024;

struct Scan {
    const LibraryRoots &roots;
    QList<LibraryItem> items;
    QHash<QString, qsizetype> byPath;

    explicit Scan(const LibraryRoots &r)
        : roots(r)
    {
    }

    QString sys(const QString &path) const
    {
        return roots.root + path;
    }

    /* One item; a path already on a shelf is not listed twice. */
    LibraryItem *add(LibraryShelf shelf, const QString &path, const QString &title, const QString &open = QString())
    {
        const QFileInfo fi(path);
        if (!fi.exists()) {
            return nullptr;
        }
        const QString key = fi.canonicalFilePath();
        if (byPath.contains(key)) {
            return &items[byPath.value(key)];
        }
        LibraryItem it;
        it.shelf = shelf;
        it.path = fi.absoluteFilePath();
        it.open = open.isEmpty() ? it.path : open;
        it.isDir = fi.isDir() && open.isEmpty();
        it.title = title;
        it.name = fi.fileName();
        it.modified = fi.lastModified();
        it.size = fi.isDir() ? dirSize(it.path) : fi.size();
        byPath.insert(key, items.size());
        items.append(it);
        return &items.last();
    }

    static qint64 dirSize(const QString &dir)
    {
        qint64 total = 0;
        int n = 0;
        QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext() && n++ < MaxSizedEntries) {
            it.next();
            total += it.fileInfo().size();
        }
        return total;
    }

    static QStringList files(const QString &dir, const QStringList &patterns, QDir::Filters kind = QDir::Files)
    {
        const QDir d(dir);
        QStringList out;
        const QStringList names = d.entryList(patterns, kind | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
        for (const QString &n : names) {
            out.append(d.filePath(n));
        }
        return out;
    }
};

QString humanName(QString base)
{
    base.replace(QLatin1Char('_'), QLatin1Char(' '));
    return base;
}

QString stripSuffix(const QString &name, const QString &suffix)
{
    return name.endsWith(suffix) ? name.left(name.size() - suffix.size()) : name;
}

/* kiwix-manage's library: <book id="…" path="…" title="…" …/>, a relative
 * path being relative to the library file. */
void readKiwixLibrary(Scan &s, const QString &file)
{
    QFile f(file);
    if (f.size() > MaxMetadataFile || !f.open(QIODevice::ReadOnly)) {
        return;
    }
    QXmlStreamReader xml(&f);
    const QDir base = QFileInfo(file).absoluteDir();
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QLatin1String("book")) {
            continue;
        }
        const auto a = xml.attributes();
        QString path = a.value(QLatin1String("path")).toString();
        if (path.isEmpty()) {
            continue;
        }
        if (QDir::isRelativePath(path)) {
            path = base.filePath(path);
        } else {
            path = s.sys(path);
        }
        QString title = a.value(QLatin1String("title")).toString().trimmed();
        if (title.isEmpty()) {
            title = humanName(stripSuffix(QFileInfo(path).fileName(), QStringLiteral(".zim")));
        }
        s.add(LibraryShelf::Books, path, title);
    }
}

void scanBooks(Scan &s)
{
    const QString kdos = s.sys(QStringLiteral("/usr/share/doc/kdos/index.html"));
    if (QFileInfo::exists(kdos)) {
        s.add(LibraryShelf::Books, s.sys(QStringLiteral("/usr/share/doc/kdos")), i18ndc(Domain, "@item a book", "The KDOS Book"), kdos);
    }
    const QString kfs = s.sys(QStringLiteral("/usr/share/doc/kfs/index.html"));
    if (QFileInfo::exists(kfs)) {
        s.add(LibraryShelf::Books, s.sys(QStringLiteral("/usr/share/doc/kfs")), i18ndc(Domain, "@item a book", "KDOS from Scratch"), kfs);
    }
    readKiwixLibrary(s, s.sys(QStringLiteral("/var/lib/kiwix/library.xml")));
    if (!s.roots.dataHome.isEmpty()) {
        readKiwixLibrary(s, s.roots.dataHome + QLatin1String("/kiwix-desktop/library.xml"));
    }
    QStringList dirs{s.sys(QStringLiteral("/usr/share/kiwix"))};
    if (!s.roots.dataHome.isEmpty()) {
        dirs.append(s.roots.dataHome + QLatin1String("/kiwix"));
    }
    for (const QString &dir : std::as_const(dirs)) {
        for (const QString &zim : Scan::files(dir, {QStringLiteral("*.zim")})) {
            s.add(LibraryShelf::Books, zim, humanName(stripSuffix(QFileInfo(zim).fileName(), QStringLiteral(".zim"))));
        }
    }
}

void scanMaps(Scan &s)
{
    for (const QString &mwm : Scan::files(s.sys(QStringLiteral("/usr/share/organicmaps/data")), {QStringLiteral("*.mwm")})) {
        s.add(LibraryShelf::Maps, mwm, humanName(stripSuffix(QFileInfo(mwm).fileName(), QStringLiteral(".mwm"))));
    }
    if (s.roots.dataHome.isEmpty()) {
        return;
    }
    /* Organic Maps keeps downloaded maps in a directory per map version. */
    const QString omaps = s.roots.dataHome + QLatin1String("/OMaps");
    QStringList dirs{omaps};
    dirs += Scan::files(omaps, {}, QDir::Dirs);
    for (const QString &dir : std::as_const(dirs)) {
        for (const QString &mwm : Scan::files(dir, {QStringLiteral("*.mwm")})) {
            s.add(LibraryShelf::Maps, mwm, humanName(stripSuffix(QFileInfo(mwm).fileName(), QStringLiteral(".mwm"))));
        }
    }
}

void scanModels(Scan &s)
{
    auto both = [&s](const QString &sub) {
        QStringList d{s.sys(QStringLiteral("/usr/share/") + sub)};
        if (!s.roots.dataHome.isEmpty()) {
            d.append(s.roots.dataHome + QLatin1Char('/') + sub);
        }
        return d;
    };
    for (const QString &dir : both(QStringLiteral("llama.cpp/models"))) {
        for (const QString &m : Scan::files(dir, {QStringLiteral("*.gguf")})) {
            s.add(LibraryShelf::Models, m,
                  i18ndc(Domain, "@item %1 a model's name", "%1 (language model)", stripSuffix(QFileInfo(m).fileName(), QStringLiteral(".gguf"))));
        }
    }
    for (const QString &dir : both(QStringLiteral("whisper.cpp/models"))) {
        for (const QString &m : Scan::files(dir, {QStringLiteral("ggml-*.bin")})) {
            const QString name = stripSuffix(QFileInfo(m).fileName().mid(5), QStringLiteral(".bin"));
            s.add(LibraryShelf::Models, m, i18ndc(Domain, "@item %1 a model's name", "Whisper %1 (speech to text)", name));
        }
    }
    for (const QString &dir : both(QStringLiteral("piper/voices"))) {
        for (const QString &m : Scan::files(dir, {QStringLiteral("*.onnx")})) {
            /* A voice is the model and its .onnx.json; one without the other
             * does not speak. */
            if (!QFileInfo::exists(m + QLatin1String(".json"))) {
                continue;
            }
            s.add(LibraryShelf::Models, m,
                  i18ndc(Domain, "@item %1 a voice's name", "Piper %1 (voice)", stripSuffix(QFileInfo(m).fileName(), QStringLiteral(".onnx"))));
        }
    }
}

QString docsetName(const QString &docset)
{
    QFile f(docset + QLatin1String("/Contents/Info.plist"));
    if (f.size() > MaxMetadataFile || !f.open(QIODevice::ReadOnly)) {
        return {};
    }
    static const QRegularExpression re(QStringLiteral("<key>\\s*CFBundleName\\s*</key>\\s*<string>([^<]*)</string>"));
    const QRegularExpressionMatch m = re.match(QString::fromUtf8(f.read(MaxMetadataFile)));
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

void scanDocsets(Scan &s)
{
    QStringList dirs{s.sys(QStringLiteral("/usr/share/zeal/docsets"))};
    if (!s.roots.dataHome.isEmpty()) {
        dirs.prepend(s.roots.dataHome + QLatin1String("/Zeal/Zeal/docsets"));
    }
    for (const QString &dir : std::as_const(dirs)) {
        for (const QString &d : Scan::files(dir, {QStringLiteral("*.docset")}, QDir::Dirs)) {
            QString title = docsetName(d);
            if (title.isEmpty()) {
                title = humanName(stripSuffix(QFileInfo(d).fileName(), QStringLiteral(".docset")));
            }
            s.add(LibraryShelf::Docsets, d, title);
        }
    }
}

void scanCourses(Scan &s)
{
    for (const QString &db : Scan::files(s.sys(QStringLiteral("/var/lib/kolibri/content/databases")), {QStringLiteral("*.sqlite3")})) {
        QString title = kdosKolibriChannelName(db);
        if (title.isEmpty()) {
            title = stripSuffix(QFileInfo(db).fileName(), QStringLiteral(".sqlite3"));
        }
        s.add(LibraryShelf::Courses, db, title);
    }
}

/* A data pack's own word on its items, from the launchers it installs. */
void readDeclared(Scan &s)
{
    QStringList dirs;
    if (!s.roots.dataHome.isEmpty()) {
        dirs.append(s.roots.dataHome + QLatin1String("/applications"));
    }
    for (const QString &d : s.roots.dataDirs) {
        dirs.append(d + QLatin1String("/applications"));
    }
    QSet<QString> seen;
    for (const QString &dir : std::as_const(dirs)) {
        for (const QString &file : Scan::files(dir, {QStringLiteral("*.desktop")})) {
            const QString id = QFileInfo(file).fileName();
            if (seen.contains(id)) {
                continue;
            }
            seen.insert(id);
            QFile f(file);
            if (f.size() > 1024 * 1024 || !f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            QHash<QString, QString> keys;
            bool inEntry = false;
            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine(4096)).trimmed();
                if (line.startsWith(QLatin1Char('['))) {
                    if (inEntry) {
                        break;
                    }
                    inEntry = line == QLatin1String("[Desktop Entry]");
                    continue;
                }
                const qsizetype eq = line.indexOf(QLatin1Char('='));
                if (inEntry && eq > 0 && !keys.contains(line.left(eq).trimmed())) {
                    keys.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
                }
            }
            LibraryShelf shelf;
            const QString path = keys.value(QStringLiteral("X-KDOS-Library-Path"));
            if (!kdosLibraryShelfFromKey(keys.value(QStringLiteral("X-KDOS-Library-Kind")), &shelf) || !path.startsWith(QLatin1Char('/'))) {
                continue;
            }
            if (keys.value(QStringLiteral("Hidden")) == QLatin1String("true")) {
                continue;
            }
            const QString name = keys.value(QStringLiteral("Name"));
            /* A declared directory with an index.html is a book: opening it
             * opens the index. */
            const QString index = s.sys(path) + QLatin1String("/index.html");
            const QString open = QFileInfo(s.sys(path)).isDir() && QFileInfo::exists(index) ? index : QString();
            LibraryItem *it = s.add(shelf, s.sys(path), name.isEmpty() ? QFileInfo(path).fileName() : name, open);
            if (!it) {
                continue;
            }
            if (!name.isEmpty()) {
                it->title = name;
            }
            it->icon = keys.value(QStringLiteral("Icon"));
            it->pack = keys.value(QStringLiteral("X-KDOS-Pack"));
        }
    }
}

qint64 stampOf(const QString &path)
{
    const QFileInfo fi(path);
    return fi.exists() ? fi.lastModified().toMSecsSinceEpoch() : 0;
}
} // namespace

QList<LibraryShelf> kdosLibraryShelves()
{
    return {LibraryShelf::Books, LibraryShelf::Maps, LibraryShelf::Models, LibraryShelf::Docsets, LibraryShelf::Courses};
}

QString kdosLibraryShelfKey(LibraryShelf shelf)
{
    switch (shelf) {
    case LibraryShelf::Books:
        return QStringLiteral("Books");
    case LibraryShelf::Maps:
        return QStringLiteral("Maps");
    case LibraryShelf::Models:
        return QStringLiteral("Models");
    case LibraryShelf::Docsets:
        return QStringLiteral("Docsets");
    case LibraryShelf::Courses:
        return QStringLiteral("Courses");
    }
    return {};
}

bool kdosLibraryShelfFromKey(const QString &key, LibraryShelf *shelf)
{
    for (LibraryShelf s : kdosLibraryShelves()) {
        if (kdosLibraryShelfKey(s) == key) {
            *shelf = s;
            return true;
        }
    }
    return false;
}

QString kdosLibraryShelfTitle(LibraryShelf shelf)
{
    switch (shelf) {
    case LibraryShelf::Books:
        return i18ndc(Domain, "@item a library shelf", "Books");
    case LibraryShelf::Maps:
        return i18ndc(Domain, "@item a library shelf", "Maps");
    case LibraryShelf::Models:
        return i18ndc(Domain, "@item a library shelf", "Models");
    case LibraryShelf::Docsets:
        return i18ndc(Domain, "@item a library shelf", "Docsets");
    case LibraryShelf::Courses:
        return i18ndc(Domain, "@item a library shelf", "Courses");
    }
    return {};
}

QString kdosLibraryShelfIcon(LibraryShelf shelf)
{
    switch (shelf) {
    case LibraryShelf::Books:
        return QStringLiteral("folder-book");
    case LibraryShelf::Maps:
        return QStringLiteral("folder-image");
    case LibraryShelf::Models:
        return QStringLiteral("folder-script");
    case LibraryShelf::Docsets:
        return QStringLiteral("folder-development");
    case LibraryShelf::Courses:
        return QStringLiteral("folder-templates");
    }
    return QStringLiteral("folder");
}

QList<LibraryItem> kdosLibraryScan(const LibraryRoots &roots)
{
    Scan s(roots);
    /* A data pack's launchers first, so its item keeps the pack's own name
     * even where a scan finds the same file. */
    readDeclared(s);
    scanBooks(s);
    scanMaps(s);
    scanModels(s);
    scanDocsets(s);
    scanCourses(s);

    QStringList unowned;
    for (const LibraryItem &it : std::as_const(s.items)) {
        if (it.pack.isEmpty() && (roots.dataHome.isEmpty() || !it.path.startsWith(roots.dataHome + QLatin1Char('/')))) {
            unowned.append(it.path);
        }
    }
    const QHash<QString, QString> owners = kdosLibraryOwners(roots, unowned);
    for (LibraryItem &it : s.items) {
        if (it.pack.isEmpty()) {
            it.pack = owners.value(it.path);
        }
    }

    /* A name is unique on its shelf: the second file of one name is
     * numbered, as a file manager names a copy. */
    QHash<QString, int> names;
    std::stable_sort(s.items.begin(), s.items.end(), [](const LibraryItem &a, const LibraryItem &b) {
        if (a.shelf != b.shelf) {
            return int(a.shelf) < int(b.shelf);
        }
        return QString::localeAwareCompare(a.title, b.title) < 0;
    });
    for (LibraryItem &it : s.items) {
        const QString key = kdosLibraryShelfKey(it.shelf) + QLatin1Char('/') + it.name;
        const int n = names.value(key);
        names.insert(key, n + 1);
        if (n) {
            it.name += QLatin1Char('~') + QString::number(n + 1);
        }
    }
    return s.items;
}

QHash<QString, QString> kdosLibraryOwners(const LibraryRoots &roots, const QStringList &paths)
{
    QHash<QString, QString> out;
    if (paths.isEmpty()) {
        return out;
    }

    /* Grafts: a data pack's files appear through a symlink under /usr/share. */
    QFile manifest(roots.root + QLatin1String("/var/lib/kdos/packs/pack-manifest"));
    QList<QPair<QString, QString>> grafts;
    if (manifest.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!manifest.atEnd()) {
            const QString line = QString::fromUtf8(manifest.readLine(8192)).trimmed();
            const qsizetype tab = line.indexOf(QLatin1Char('\t'));
            if (tab > 0) {
                grafts.append({line.left(tab), roots.root + line.mid(tab + 1)});
            }
        }
    }

    /* The rest by kpkg's record: each package's file lists every path it
     * installed as `./usr/…`. */
    QHash<QByteArray, QString> wanted;
    for (const QString &p : paths) {
        bool grafted = false;
        for (const auto &[pack, at] : std::as_const(grafts)) {
            if (p == at || p.startsWith(at + QLatin1Char('/'))) {
                out.insert(p, pack);
                grafted = true;
                break;
            }
        }
        if (!grafted && p.startsWith(roots.root + QLatin1Char('/'))) {
            wanted.insert(QByteArray(".") + QFile::encodeName(p.mid(roots.root.size())), p);
        }
    }
    if (wanted.isEmpty()) {
        return out;
    }

    const QString db = roots.root + QLatin1String("/var/lib/kpkg/db");
    const QByteArray stamp = "kdos-library-owners 1 " + QByteArray::number(stampOf(db));
    QHash<QByteArray, QString> cached;
    bool cacheValid = false;
    if (!roots.ownerCache.isEmpty()) {
        QFile c(roots.ownerCache);
        if (c.open(QIODevice::ReadOnly) && c.readLine().trimmed() == stamp) {
            cacheValid = true;
            while (!c.atEnd()) {
                const QByteArray line = c.readLine().chopped(1);
                const qsizetype tab = line.indexOf('\t');
                if (tab > 0) {
                    cached.insert(line.left(tab), QString::fromUtf8(line.mid(tab + 1)));
                }
            }
        }
    }

    QHash<QByteArray, QString> found;
    QSet<QByteArray> missing;
    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
        if (cacheValid && cached.contains(it.key())) {
            found.insert(it.key(), cached.value(it.key()));
        } else {
            missing.insert(it.key());
        }
    }

    if (!missing.isEmpty()) {
        const QSet<QByteArray> looked = missing;
        const QDir d(db);
        const QStringList pkgs = d.entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
        for (const QString &pkg : pkgs) {
            if (missing.isEmpty()) {
                break;
            }
            QFile f(d.filePath(pkg));
            if (!f.open(QIODevice::ReadOnly)) {
                continue;
            }
            while (!f.atEnd() && !missing.isEmpty()) {
                QByteArray line = f.readLine();
                if (line.endsWith('\n')) {
                    line.chop(1);
                }
                if (missing.remove(line)) {
                    found.insert(line, pkg);
                }
            }
        }
        /* What was looked for and not found is remembered as unowned, so the
         * next listing does not read every package again for it. */
        for (const QByteArray &k : looked) {
            if (!found.contains(k)) {
                found.insert(k, QString());
            }
        }
        if (!roots.ownerCache.isEmpty()) {
            QDir().mkpath(QFileInfo(roots.ownerCache).absolutePath());
            QSaveFile c(roots.ownerCache);
            if (c.open(QIODevice::WriteOnly)) {
                c.write(stamp + '\n');
                QHash<QByteArray, QString> all = cacheValid ? cached : QHash<QByteArray, QString>();
                all.insert(found);
                for (auto it = all.cbegin(); it != all.cend(); ++it) {
                    c.write(it.key() + '\t' + it.value().toUtf8() + '\n');
                }
                c.commit();
            }
        }
    }

    for (auto it = found.cbegin(); it != found.cend(); ++it) {
        if (!it.value().isEmpty()) {
            out.insert(wanted.value(it.key()), it.value());
        }
    }
    return out;
}

QString kdosKolibriChannelName(const QString &database)
{
    QString name;
    const QString conn = QStringLiteral("kdos-library-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(database);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=200"));
        if (db.open()) {
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("SELECT name FROM content_channelmetadata LIMIT 1")) && q.next()) {
                name = q.value(0).toString().trimmed();
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    return name;
}

} // namespace Kdos
