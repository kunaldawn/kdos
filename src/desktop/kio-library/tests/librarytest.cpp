/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   librarytest — the library of a machine built in a temporary directory
 */

#include "library.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace Kdos;

namespace
{
void put(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(data);
    }
}

const LibraryItem *find(const QList<LibraryItem> &items, LibraryShelf shelf, const QString &name)
{
    for (const LibraryItem &it : items) {
        if (it.shelf == shelf && it.name == name) {
            return &it;
        }
    }
    return nullptr;
}
} // namespace

class LibraryTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_t;
    LibraryRoots m_roots;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_t.isValid());
        const QString r = m_t.path() + QStringLiteral("/root");
        const QString home = m_t.path() + QStringLiteral("/home/.local/share");
        m_roots.root = r;
        m_roots.dataHome = home;
        m_roots.dataDirs = {m_t.path() + QStringLiteral("/sysdata")};
        m_roots.ownerCache = m_t.path() + QStringLiteral("/cache/library-owners");

        put(r + QStringLiteral("/usr/share/doc/kdos/index.html"), "<html>book</html>");
        put(r + QStringLiteral("/usr/share/doc/kdos/01-philosophy/why-kdos.html"), QByteArray(1000, 'x'));
        put(r + QStringLiteral("/usr/share/kiwix/wikipedia_en_all.zim"), QByteArray(4096, 'z'));
        put(r + QStringLiteral("/srv/zim/gutenberg.zim"), QByteArray(10, 'g'));
        put(r + QStringLiteral("/var/lib/kiwix/library.xml"),
            "<library version=\"20110515\">"
            "<book id=\"a\" path=\"../../../srv/zim/gutenberg.zim\" title=\"Project Gutenberg\"/>"
            "<book id=\"b\" path=\"/usr/share/kiwix/wikipedia_en_all.zim\" title=\"Wikipedia\"/>"
            "<book id=\"c\" path=\"/nowhere/gone.zim\" title=\"Gone\"/>"
            "</library>");
        put(r + QStringLiteral("/usr/share/organicmaps/data/World.mwm"), QByteArray(300, 'w'));
        put(home + QStringLiteral("/OMaps/260101/Germany_Berlin.mwm"), QByteArray(30, 'b'));
        put(r + QStringLiteral("/usr/share/llama.cpp/models/qwen3-4b.gguf"), QByteArray(50, 'q'));
        put(r + QStringLiteral("/usr/share/whisper.cpp/models/ggml-base.en.bin"), QByteArray(60, 'm'));
        put(home + QStringLiteral("/piper/voices/en_US-lessac-medium.onnx"), "v");
        put(home + QStringLiteral("/piper/voices/en_US-lessac-medium.onnx.json"), "{}");
        put(home + QStringLiteral("/piper/voices/orphan.onnx"), "v");
        put(home + QStringLiteral("/Zeal/Zeal/docsets/Qt_6.docset/Contents/Info.plist"),
            "<plist><dict><key>CFBundleIdentifier</key><string>qt</string><key>CFBundleName</key><string>Qt 6</string></dict></plist>");
        put(home + QStringLiteral("/Zeal/Zeal/docsets/Qt_6.docset/Contents/Resources/docSet.dsidx"), QByteArray(70, 'd'));

        const QString kolibri = r + QStringLiteral("/var/lib/kolibri/content/databases/0123abcd.sqlite3");
        QDir().mkpath(QFileInfo(kolibri).absolutePath());
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("make"));
            db.setDatabaseName(kolibri);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE content_channelmetadata (id TEXT, name TEXT)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO content_channelmetadata VALUES ('0123abcd', 'Khan Academy (English)')")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("make"));
        put(r + QStringLiteral("/var/lib/kolibri/content/databases/broken.sqlite3"), "not a database");

        /* A data pack's launcher names its ZIM and its pack. */
        put(m_t.path() + QStringLiteral("/sysdata/applications/kdos-library-wikipedia.desktop"),
            "[Desktop Entry]\nType=Application\nName=Wikipedia (offline)\nIcon=wikipedia\nCategories=X-KDOS-Library;\n"
            "X-KDOS-Library-Kind=Books\nX-KDOS-Library-Path=/usr/share/kiwix/wikipedia_en_all.zim\nX-KDOS-Pack=data.wikipedia-en\n");
        put(m_t.path() + QStringLiteral("/sysdata/applications/kdos-library-bad.desktop"),
            "[Desktop Entry]\nName=Bad\nX-KDOS-Library-Kind=Shelves\nX-KDOS-Library-Path=/usr/share/kiwix/wikipedia_en_all.zim\n");

        /* Who owns what: a graft for the docs, kpkg for the rest. */
        put(r + QStringLiteral("/var/lib/kdos/packs/pack-manifest"), "data.kdos-books\t/usr/share/doc/kdos\n");
        put(r + QStringLiteral("/var/lib/kpkg/db/organicmaps"), "2026.01 1\n./\n./usr/\n./usr/share/organicmaps/data/World.mwm\n");
        put(r + QStringLiteral("/var/lib/kpkg/db/whisper-model-base-en"), "1.0 1\n./usr/share/whisper.cpp/models/ggml-base.en.bin\n");
        QDir().mkpath(r + QStringLiteral("/var/lib/kpkg/db/.files"));
    }

    void shelves()
    {
        QCOMPARE(kdosLibraryShelves().size(), 5);
        LibraryShelf s;
        QVERIFY(kdosLibraryShelfFromKey(QStringLiteral("Docsets"), &s));
        QCOMPARE(s, LibraryShelf::Docsets);
        QVERIFY(!kdosLibraryShelfFromKey(QStringLiteral("books"), &s));
        for (LibraryShelf x : kdosLibraryShelves()) {
            QVERIFY(!kdosLibraryShelfTitle(x).isEmpty());
            QVERIFY(!kdosLibraryShelfIcon(x).isEmpty());
        }
    }

    void scansEveryShelf()
    {
        const QList<LibraryItem> items = kdosLibraryScan(m_roots);

        const LibraryItem *book = find(items, LibraryShelf::Books, QStringLiteral("kdos"));
        QVERIFY(book);
        QCOMPARE(book->title, QStringLiteral("The KDOS Book"));
        QVERIFY(book->open.endsWith(QStringLiteral("/usr/share/doc/kdos/index.html")));
        QVERIFY(!book->isDir);
        QCOMPARE(book->size, 1017);
        QCOMPARE(book->pack, QStringLiteral("data.kdos-books"));

        const LibraryItem *wiki = find(items, LibraryShelf::Books, QStringLiteral("wikipedia_en_all.zim"));
        QVERIFY(wiki);
        QCOMPARE(wiki->title, QStringLiteral("Wikipedia (offline)"));
        QCOMPARE(wiki->pack, QStringLiteral("data.wikipedia-en"));
        QCOMPARE(wiki->icon, QStringLiteral("wikipedia"));
        QCOMPARE(wiki->size, 4096);

        const LibraryItem *gut = find(items, LibraryShelf::Books, QStringLiteral("gutenberg.zim"));
        QVERIFY(gut);
        QCOMPARE(gut->title, QStringLiteral("Project Gutenberg"));
        QVERIFY(!find(items, LibraryShelf::Books, QStringLiteral("gone.zim")));

        const LibraryItem *world = find(items, LibraryShelf::Maps, QStringLiteral("World.mwm"));
        QVERIFY(world);
        QCOMPARE(world->pack, QStringLiteral("organicmaps"));
        const LibraryItem *berlin = find(items, LibraryShelf::Maps, QStringLiteral("Germany_Berlin.mwm"));
        QVERIFY(berlin);
        QCOMPARE(berlin->title, QStringLiteral("Germany Berlin"));
        QVERIFY(berlin->pack.isEmpty());

        QVERIFY(find(items, LibraryShelf::Models, QStringLiteral("qwen3-4b.gguf")));
        const LibraryItem *whisper = find(items, LibraryShelf::Models, QStringLiteral("ggml-base.en.bin"));
        QVERIFY(whisper);
        QCOMPARE(whisper->title, QStringLiteral("Whisper base.en (speech to text)"));
        QCOMPARE(whisper->pack, QStringLiteral("whisper-model-base-en"));
        QVERIFY(find(items, LibraryShelf::Models, QStringLiteral("en_US-lessac-medium.onnx")));
        QVERIFY(!find(items, LibraryShelf::Models, QStringLiteral("orphan.onnx")));

        const LibraryItem *qt = find(items, LibraryShelf::Docsets, QStringLiteral("Qt_6.docset"));
        QVERIFY(qt);
        QCOMPARE(qt->title, QStringLiteral("Qt 6"));
        QVERIFY(qt->isDir);
        QCOMPARE(qt->size, QFileInfo(qt->path + QStringLiteral("/Contents/Info.plist")).size() + 70);

        const LibraryItem *khan = find(items, LibraryShelf::Courses, QStringLiteral("0123abcd.sqlite3"));
        QVERIFY(khan);
        QCOMPARE(khan->title, QStringLiteral("Khan Academy (English)"));
        const LibraryItem *broken = find(items, LibraryShelf::Courses, QStringLiteral("broken.sqlite3"));
        QVERIFY(broken);
        QCOMPARE(broken->title, QStringLiteral("broken"));

        /* Shelves in order, titles in order within each. */
        for (qsizetype i = 1; i < items.size(); ++i) {
            QVERIFY(int(items[i - 1].shelf) <= int(items[i].shelf));
        }
    }

    void ownerCacheIsReused()
    {
        QVERIFY(QFile::exists(m_roots.ownerCache));
        /* With the record emptied in place, which leaves the database
         * directory's stamp as it was, the cache still answers. */
        const QString rec = m_roots.root + QStringLiteral("/var/lib/kpkg/db/organicmaps");
        QFile f(rec);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray was = f.readAll();
        f.close();
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("2026.01 1\n");
        f.close();
        const QHash<QString, QString> o = kdosLibraryOwners(m_roots, {m_roots.root + QStringLiteral("/usr/share/organicmaps/data/World.mwm")});
        QCOMPARE(o.value(m_roots.root + QStringLiteral("/usr/share/organicmaps/data/World.mwm")), QStringLiteral("organicmaps"));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(was);
    }

    void namesStayUniqueOnAShelf()
    {
        QTemporaryDir t;
        LibraryRoots r;
        r.root = t.path();
        r.dataHome = t.path() + QStringLiteral("/home");
        put(t.path() + QStringLiteral("/usr/share/kiwix/a.zim"), "1");
        put(r.dataHome + QStringLiteral("/kiwix/a.zim"), "22");
        const QList<LibraryItem> items = kdosLibraryScan(r);
        QCOMPARE(items.size(), 2);
        QStringList names{items[0].name, items[1].name};
        names.sort();
        QCOMPARE(names, QStringList({QStringLiteral("a.zim"), QStringLiteral("a.zim~2")}));
    }

    void emptyMachine()
    {
        QTemporaryDir t;
        LibraryRoots r;
        r.root = t.path();
        QVERIFY(kdosLibraryScan(r).isEmpty());
    }
};

QTEST_GUILESS_MAIN(LibraryTest)

#include "librarytest.moc"
