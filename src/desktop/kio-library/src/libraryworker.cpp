/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kio_library — library:/, the offline library as a folder
 *
 *     library:/                 Books, Maps, Models, Docsets, Courses
 *     library:/Books/<name>     an item: its size, its pack, and the file
 *                               it opens as its local path
 *
 * Nothing here is written: every item is a file another package or pack
 * owns, and the worker only says where it is. An item's entry carries the
 * file's own local path and file:// target, so opening it opens the file
 * itself with the program the menu would choose, and a directory item is
 * entered as the directory it is.
 */

#include <KIO/WorkerBase>
#include <KLocalizedString>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QUrl>

#include <sys/stat.h>

#include "library.h"

using namespace Kdos;

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.library" FILE "library.json")
};

namespace
{

/* A file manager stats each item it lists, and a scan sums every file of a
 * book or docset: one scan answers every request for this long. */
constexpr qint64 ScanReuseMs = 5 * 1000;

LibraryRoots machineRoots()
{
    LibraryRoots r;
    r.root = qEnvironmentVariable("KDOS_LIBRARY_ROOT");
    r.dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QStringList all = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &d : all) {
        if (d != r.dataHome) {
            r.dataDirs.append(d);
        }
    }
    r.ownerCache = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/kdos/library-owners");
    return r;
}

KIO::UDSEntry shelfEntry(LibraryShelf shelf)
{
    KIO::UDSEntry e;
    e.reserve(6);
    e.fastInsert(KIO::UDSEntry::UDS_NAME, kdosLibraryShelfKey(shelf));
    e.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, kdosLibraryShelfTitle(shelf));
    e.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    e.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0555);
    e.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    e.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, kdosLibraryShelfIcon(shelf));
    return e;
}

KIO::UDSEntry rootEntry()
{
    KIO::UDSEntry e;
    e.reserve(6);
    e.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    e.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18ndc("kdos-kio-library", "@title the library folder", "Library"));
    e.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    e.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0555);
    e.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    e.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("folder-library"));
    return e;
}

KIO::UDSEntry itemEntry(const LibraryItem &it)
{
    static const QMimeDatabase mimes;
    KIO::UDSEntry e;
    e.reserve(11);
    e.fastInsert(KIO::UDSEntry::UDS_NAME, it.name);
    e.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, it.title);
    e.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, it.isDir ? S_IFDIR : S_IFREG);
    e.fastInsert(KIO::UDSEntry::UDS_ACCESS, it.isDir ? 0555 : 0444);
    e.fastInsert(KIO::UDSEntry::UDS_SIZE, it.size);
    if (it.modified.isValid()) {
        e.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, it.modified.toSecsSinceEpoch());
    }
    e.fastInsert(KIO::UDSEntry::UDS_LOCAL_PATH, it.open);
    e.fastInsert(KIO::UDSEntry::UDS_TARGET_URL, QUrl::fromLocalFile(it.open).toString());
    e.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, it.isDir ? QStringLiteral("inode/directory") : mimes.mimeTypeForFile(it.open).name());
    if (!it.icon.isEmpty()) {
        e.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, it.icon);
    }
    e.fastInsert(KIO::UDSEntry::UDS_EXTRA, it.pack);
    return e;
}

/* library:/<shelf>[/<name>[/…]] split into its parts; false for any other
 * shape. A shelf key is case-sensitive, as every URL path is. */
bool parse(const QUrl &url, LibraryShelf *shelf, QString *name, QString *rest, bool *isRoot)
{
    const QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    *isRoot = parts.isEmpty();
    if (parts.isEmpty()) {
        return true;
    }
    if (!kdosLibraryShelfFromKey(parts.first(), shelf)) {
        return false;
    }
    *name = parts.size() > 1 ? parts.at(1) : QString();
    *rest = parts.size() > 2 ? parts.mid(2).join(QLatin1Char('/')) : QString();
    return true;
}

} // namespace

class LibraryWorker : public KIO::WorkerBase
{
public:
    LibraryWorker(const QByteArray &pool, const QByteArray &app)
        : KIO::WorkerBase(QByteArrayLiteral("library"), pool, app)
    {
    }

    const QList<LibraryItem> &items()
    {
        if (!m_scanAge.isValid() || m_scanAge.elapsed() >= ScanReuseMs) {
            m_items = kdosLibraryScan(machineRoots());
            m_scanAge.start();
        }
        return m_items;
    }

    KIO::WorkerResult listDir(const QUrl &url) override
    {
        LibraryShelf shelf;
        QString name, rest;
        bool isRoot;
        if (!parse(url, &shelf, &name, &rest, &isRoot)) {
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
        }
        if (isRoot) {
            const QList<LibraryShelf> shelves = kdosLibraryShelves();
            totalSize(shelves.size() + 1);
            listEntry(rootEntry());
            for (LibraryShelf s : shelves) {
                listEntry(shelfEntry(s));
            }
            return KIO::WorkerResult::pass();
        }
        /* Listing a shelf always scans afresh, so a pack installed a moment
         * ago is on its shelf when the shelf is opened or reloaded. */
        if (name.isEmpty()) {
            m_scanAge.invalidate();
        }
        const QList<LibraryItem> &items = this->items();
        if (name.isEmpty()) {
            KIO::UDSEntry dot = shelfEntry(shelf);
            dot.replace(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
            listEntry(dot);
            for (const LibraryItem &it : items) {
                if (it.shelf == shelf) {
                    listEntry(itemEntry(it));
                }
            }
            return KIO::WorkerResult::pass();
        }
        /* Inside a directory item is the directory itself. */
        for (const LibraryItem &it : items) {
            if (it.shelf == shelf && it.name == name && it.isDir) {
                QUrl target = QUrl::fromLocalFile(it.path);
                if (!rest.isEmpty()) {
                    target.setPath(target.path() + QLatin1Char('/') + rest);
                }
                redirection(target);
                return KIO::WorkerResult::pass();
            }
        }
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }

    KIO::WorkerResult stat(const QUrl &url) override
    {
        LibraryShelf shelf;
        QString name, rest;
        bool isRoot;
        if (!parse(url, &shelf, &name, &rest, &isRoot)) {
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
        }
        if (isRoot) {
            statEntry(rootEntry());
            return KIO::WorkerResult::pass();
        }
        if (name.isEmpty()) {
            statEntry(shelfEntry(shelf));
            return KIO::WorkerResult::pass();
        }
        for (const LibraryItem &it : items()) {
            if (it.shelf != shelf || it.name != name) {
                continue;
            }
            if (!rest.isEmpty()) {
                if (!it.isDir) {
                    break;
                }
                redirection(QUrl::fromLocalFile(it.path + QLatin1Char('/') + rest));
                return KIO::WorkerResult::pass();
            }
            statEntry(itemEntry(it));
            return KIO::WorkerResult::pass();
        }
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }

    KIO::WorkerResult get(const QUrl &url) override
    {
        LibraryShelf shelf;
        QString name, rest;
        bool isRoot;
        if (!parse(url, &shelf, &name, &rest, &isRoot) || isRoot || name.isEmpty()) {
            return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, url.toDisplayString());
        }
        for (const LibraryItem &it : items()) {
            if (it.shelf == shelf && it.name == name) {
                if (it.isDir && rest.isEmpty()) {
                    return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, url.toDisplayString());
                }
                redirection(QUrl::fromLocalFile(rest.isEmpty() ? it.open : it.path + QLatin1Char('/') + rest));
                return KIO::WorkerResult::pass();
            }
        }
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }

private:
    QList<LibraryItem> m_items;
    QElapsedTimer m_scanAge;
};

extern "C" {
int Q_DECL_EXPORT kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_library"));
    if (argc != 4) {
        fprintf(stderr, "Usage: kio_library protocol domain-socket1 domain-socket2\n");
        return 1;
    }
    LibraryWorker worker(argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}
}

#include "libraryworker.moc"
