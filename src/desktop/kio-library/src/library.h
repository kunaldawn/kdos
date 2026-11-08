/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   library — what the offline library holds, shelf by shelf
 *
 * Everything library:/ lists, found where each program keeps its data, as a
 * plain scan over a root directory and a home, so the QtTest suite builds a
 * fake machine in a temporary directory and no test needs a ZIM, a model or
 * kpkg's database.
 *
 *   Books    ZIM archives (kiwix-manage's library and the kiwix directories),
 *            the KDOS book and KDOS from Scratch
 *   Maps     Organic Maps' .mwm files
 *   Models   llama.cpp's .gguf, whisper.cpp's ggml-*.bin, Piper's .onnx voices
 *   Docsets  Zeal's .docset directories
 *   Courses  Kolibri's channel databases
 *
 * A data pack names its own items with a launcher carrying
 * `X-KDOS-Library-Kind=<shelf>` and `X-KDOS-Library-Path=<file>`: the
 * launcher's Name, Icon and `X-KDOS-Pack` then win over what the scan found,
 * and the same launcher is the item's entry in the menu's Library category.
 */

#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace Kdos
{

enum class LibraryShelf {
    Books,
    Maps,
    Models,
    Docsets,
    Courses,
};

struct LibraryItem {
    LibraryShelf shelf = LibraryShelf::Books;
    /* The item's name in its shelf's URL: the file's own name, unique in the
     * shelf. */
    QString name;
    QString title;
    /* What the item is: a file, or a directory for a book or a docset. */
    QString path;
    /* What opening it opens: the path itself, or a book's index.html. */
    QString open;
    bool isDir = false;
    qint64 size = 0;
    QDateTime modified;
    QString pack;
    QString icon;
};

struct LibraryRoots {
    /* Prepended to every system path; empty on a real machine. */
    QString root;
    /* $XDG_DATA_HOME. */
    QString dataHome;
    /* $XDG_DATA_DIRS, the system's applications directories' parents. */
    QStringList dataDirs;
    /* Where the pack-owner index is kept between runs; empty for none. */
    QString ownerCache;
};

/* The shelves in the order library:/ lists them. */
QList<LibraryShelf> kdosLibraryShelves();
/* A shelf's name in a URL: Books, Maps, Models, Docsets, Courses. */
QString kdosLibraryShelfKey(LibraryShelf shelf);
/* The shelf a URL segment names; false for none. */
bool kdosLibraryShelfFromKey(const QString &key, LibraryShelf *shelf);
QString kdosLibraryShelfTitle(LibraryShelf shelf);
QString kdosLibraryShelfIcon(LibraryShelf shelf);

/* Every item on every shelf, sorted by shelf, then title. */
QList<LibraryItem> kdosLibraryScan(const LibraryRoots &roots);

/* The pack or package each of PATHS came from: a graft recorded in
 * /var/lib/kdos/packs/pack-manifest first, then the kpkg package whose file list
 * names the path. Paths owned by neither are absent from the answer. */
QHash<QString, QString> kdosLibraryOwners(const LibraryRoots &roots, const QStringList &paths);

/* A Kolibri channel database's channel name; empty when it cannot be read. */
QString kdosKolibriChannelName(const QString &database);

} // namespace Kdos
