/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

#include "boxlistmodel.h"

#include "boxprofile.h"
#include "boxtool.h"
#include "confinement.h"

#include <KApplicationTrader>
#include <KFormat>
#include <KLocalizedString>
#include <KService>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Kdos
{

namespace
{
/* Podman is asked again this often while the page is open: a box started
 * by launching its application changes no file this page watches until its
 * runtime records its state. */
constexpr int PollMs = 15000;

/* kdos-appbox writes a reason as a clause ("the box shares the session's
 * runtime directory, …"); the page sets it after the verdict's own full
 * stop, so it is given a capital and a full stop of its own. Only a plain
 * word takes the capital, the rule the profile's notes follow: a program
 * name, a path or a profile key ("wayland = no, …") keeps its spelling. */
QString asSentence(const QString &clause)
{
    static const QRegularExpression plainWord(u"^[a-z]+\\b(?![/.=_-])(?!\\s*=)"_s);
    QString s = clause.trimmed();
    if (s.isEmpty()) {
        return s;
    }
    if (plainWord.match(s).hasMatch()) {
        s[0] = s.at(0).toUpper();
    }
    if (!s.endsWith(u'.') && !s.endsWith(u'!') && !s.endsWith(u'?')) {
        s += u'.';
    }
    return s;
}

/* `kdos-box remove` puts this line at the top of a profile it keeps
 * (BOX_KEPT_MARK in box_cmd.c); the next create's save drops it. */
constexpr QLatin1StringView KeptMark("# kept by kdos-box remove");

/* An exported launcher is named `<Name> (<box>)`; the subtitle names the
 * program alone, the box being already the row. */
QString programName(const QString &entryName, const QString &box)
{
    const QString suffix = u" ("_s + box + u')';
    return entryName.endsWith(suffix) ? entryName.chopped(suffix.size()) : entryName;
}

}

bool BoxRow::isRunning() const
{
    return state == "running"_L1;
}

bool BoxRow::hasContainer() const
{
    return !state.isEmpty() && state != "not created"_L1 && state != "absent"_L1;
}

bool BoxRow::isAppBox() const
{
    /* A pack box over its own pack, a store box over the image built for
     * it (box_lane's `image:kdos/`), or an application's box whose first
     * launch has not recorded its pack yet. */
    return base == u"pack:"_s + name || base == u"image:kdos/"_s + name || (base.isEmpty() && name.startsWith(u"app."_s));
}

QString BoxRow::exportId() const
{
    /* `kdos-appbox export` takes a catalogue id: the pack a pack box is
     * made of, which is also the name of the one box per application. */
    if (base.startsWith(u"pack:"_s)) {
        return base.mid(5);
    }
    if (name.startsWith(u"app."_s)) {
        return name;
    }
    return QString();
}

QString BoxRow::diskText() const
{
    return diskBytes > 0 ? KFormat().formatByteSize(double(diskBytes), 1, KFormat::IECBinaryDialect) : QString();
}

BoxListModel::BoxListModel(BoxTool *tool, QObject *parent)
    : QAbstractListModel(parent)
    , m_tool(tool)
    , m_watcher(new QFileSystemWatcher(this))
    , m_debounce(new QTimer(this))
    , m_poll(new QTimer(this))
{
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(400);
    connect(m_debounce, &QTimer::timeout, this, &BoxListModel::refresh);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_debounce, qOverload<>(&QTimer::start));
    m_poll->setInterval(PollMs);
    connect(m_poll, &QTimer::timeout, this, &BoxListModel::refresh);
    m_poll->start();
}

int BoxListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant BoxListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const BoxRow &r = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return r.displayName;
    case Qt::DecorationRole:
        return r.iconName;
    case NameRole:
        return r.name;
    case BaseRole:
        return r.base;
    case StateRole:
        return r.state;
    case StateTextRole:
        return stateText(r.state, r.kept);
    case RunningRole:
        return r.isRunning();
    case CreatedRole:
        return r.hasContainer();
    case PersistenceRole:
        return r.persistence;
    case AccentRole:
        return r.accent;
    case DiskRole:
        return r.diskText();
    case ConfinementRole:
        return r.confinement;
    case ConfinementTextRole:
        return confinementText(r.confinement);
    case ConfinementReasonRole:
        return r.confinementReason;
    case SubtitleRole: {
        QStringList parts{stateText(r.state, r.kept)};
        if (r.isRunning() && !r.confinement.isEmpty()) {
            parts << confinementText(r.confinement);
        }
        if (r.displayName != r.name) {
            parts << r.name;
        }
        if (!r.exports.isEmpty()) {
            /* Two programs by name, and an ellipsis for the rest. */
            QStringList shown = r.exports.mid(0, 2);
            if (r.exports.size() > 2) {
                shown << u"…"_s;
            }
            parts << i18nc("@info %1 the programs exported from a box", "Box for %1", shown.join(u", "_s));
        }
        return parts.join(u" · "_s);
    }
    case ExportIdRole:
        return r.exportId();
    case KeptRole:
        return r.kept;
    }
    return {};
}

QHash<int, QByteArray> BoxListModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {Qt::DecorationRole, "decoration"},
        {NameRole, "name"},
        {BaseRole, "base"},
        {StateRole, "boxState"},
        {StateTextRole, "stateText"},
        {RunningRole, "running"},
        {CreatedRole, "created"},
        {PersistenceRole, "persistence"},
        {AccentRole, "accent"},
        {DiskRole, "disk"},
        {ConfinementRole, "confinement"},
        {ConfinementTextRole, "confinementText"},
        {ConfinementReasonRole, "confinementReason"},
        {SubtitleRole, "subtitle"},
        {ExportIdRole, "exportId"},
        {KeptRole, "kept"},
    };
}

bool BoxListModel::isLoaded() const
{
    return m_loaded;
}

QString BoxListModel::error() const
{
    return m_error;
}

int BoxListModel::indexOf(const QString &name) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).name == name) {
            return i;
        }
    }
    return -1;
}

const BoxRow *BoxListModel::row(const QString &name) const
{
    const int i = indexOf(name);
    return i < 0 ? nullptr : &m_rows.at(i);
}

QString BoxListModel::stateText(const QString &state, bool kept)
{
    if (state == "running"_L1) {
        return i18nc("@info box state", "Running");
    }
    if (state == "not created"_L1 || state.isEmpty() || state == "absent"_L1) {
        return kept ? i18nc("@info box state", "Removed — settings kept") : i18nc("@info box state", "Not created");
    }
    if (state == "exited"_L1 || state == "stopped"_L1 || state == "created"_L1 || state == "configured"_L1) {
        return i18nc("@info box state", "Stopped");
    }
    if (state == "paused"_L1) {
        return i18nc("@info box state", "Paused");
    }
    if (state == "stopping"_L1) {
        return i18nc("@info box state", "Stopping");
    }
    return state;
}

QString BoxListModel::confinementText(const QString &confinement)
{
    if (confinement == "confined"_L1) {
        return i18nc("@info box confinement", "Confined");
    }
    if (confinement == "tagged"_L1) {
        return i18nc("@info box confinement", "Tagged");
    }
    if (confinement == "unconfined"_L1) {
        return i18nc("@info box confinement", "Not confined");
    }
    return QString();
}

QList<BoxRow> BoxListModel::parseListing(const QString &output)
{
    QList<BoxRow> rows;
    const QJsonArray arr = QJsonDocument::fromJson(output.toUtf8()).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        BoxRow r;
        r.name = o.value(u"name"_s).toString();
        if (!BoxTool::isValidBoxName(r.name)) {
            continue;
        }
        r.base = o.value(u"base"_s).toString();
        r.state = o.value(u"state"_s).toString();
        r.persistence = o.value(u"persistence"_s).toString();
        r.accent = o.value(u"accent"_s).toString();
        r.diskBytes = qMax<qint64>(0, qint64(o.value(u"disk"_s).toDouble()));
        const qint64 created = qint64(o.value(u"created"_s).toDouble());
        if (created > 0) {
            r.created = QDateTime::fromSecsSinceEpoch(created);
        }
        /* A distrobox with no profile is not kdos-box's (kdos-fetch-app
         * makes one) and is entered with `distrobox enter`, which shares
         * the home, the session's runtime directory, its display and
         * its bus: this is the verdict it reads as. kdos-box makes its own
         * boxes with `distrobox create` too, so they carry the same label,
         * but each has a profile that may unshare any of those, and one
         * started by `kdos-box enter` writes no record: it stays unknown.
         * decorate() puts a record's verdict in place of this one. */
        if (o.value(u"manager"_s).toString() == "distrobox"_L1 && !QFileInfo::exists(boxProfileDir() + u'/' + r.name + u".conf"_s)) {
            r.confinement = u"unconfined"_s;
            r.confinementReason = i18nc("@info why a box is not confined",
                                        "It is a distrobox, which shares your home and the session's runtime directory, display and bus.");
        }
        r.kept = !r.hasContainer() && profileIsKept(r.name);
        /* Only a store box's row carries `image`; "absent" is an image
         * its application's uninstall released, which no create fetches. */
        r.imageMissing = o.value(u"image"_s).toString() == "absent"_L1;
        rows << r;
    }
    return rows;
}

bool BoxListModel::profileIsKept(const QString &box)
{
    QFile f(boxProfileDir() + u'/' + box + u".conf"_s);
    return f.open(QIODevice::ReadOnly) && f.readLine(256).startsWith(QByteArrayView(KeptMark));
}

void BoxListModel::nameRow(BoxRow &row, const QList<BoxEntry> &entries)
{
    row.displayName = row.name;
    row.iconName = u"preferences-security"_s;
    row.exports.clear();

    bool named = false;
    bool iconed = false;
    for (const BoxEntry &e : entries) {
        if (e.exported) {
            if (!e.name.isEmpty()) {
                row.exports << programName(e.name, row.name);
            }
        } else if (row.isAppBox() && !named && !e.name.isEmpty()) {
            row.displayName = e.name;
            named = true;
            if (!e.icon.isEmpty()) {
                row.iconName = e.icon;
                iconed = true;
            }
        }
    }
    if (!iconed) {
        for (const BoxEntry &e : entries) {
            if (!e.icon.isEmpty()) {
                row.iconName = e.icon;
                break;
            }
        }
    }
    /* An application's box whose application has gone is still named
     * after it, without the catalogue's `app.` prefix; the subtitle keeps
     * the box's own name. */
    if (!named && row.isAppBox() && row.name.startsWith(u"app."_s) && row.name.size() > 4) {
        row.displayName = row.name.mid(4);
    }
}

void BoxListModel::decorate(BoxRow &row) const
{
    /* The box's own entry first, then every entry that names it. */
    KService::List services;
    if (KService::Ptr own = KService::serviceByDesktopName(row.name)) {
        services << own;
    }
    const QString box = row.name;
    services += KApplicationTrader::query([&box](const KService::Ptr &s) {
        return s->property<QString>(u"X-KDOS-Box"_s) == box;
    });
    QList<BoxEntry> entries;
    QStringList seen;
    for (const KService::Ptr &s : std::as_const(services)) {
        if (seen.contains(s->storageId())) {
            continue;
        }
        seen << s->storageId();
        entries.append({s->desktopEntryName(), s->name(), s->icon(), s->property<QString>(u"X-KDOS-Export"_s) == "true"_L1});
    }
    nameRow(row, entries);
    if (!row.isRunning()) {
        row.confinement.clear();
        row.confinementReason.clear();
        return;
    }
    const ConfinementRecord rec = readConfinementRecord(row.name);
    if (rec.state != Confinement::Unknown || row.confinement.isEmpty()) {
        row.confinement = confinementId(rec.state);
        row.confinementReason = rec.state == Confinement::Confined ? QString() : asSentence(rec.reason);
    }
}

void BoxListModel::watchPaths()
{
    const QStringList want{boxProfileDir(), kdosRuntimeDir() + u"/kdos/boxes"_s};
    for (const QString &p : want) {
        if (!m_watcher->directories().contains(p) && QDir(p).exists()) {
            m_watcher->addPath(p);
        }
    }
}

void BoxListModel::refresh()
{
    /* One listing at a time; a request made during one runs once after it,
     * so a burst of file changes costs two podman calls, not twenty. */
    if (m_pending) {
        m_again = true;
        return;
    }
    m_pending = true;
    m_tool->run(u"kdos-box"_s, {u"list"_s, u"--json"_s}, [this](const BoxTool::Result &r) {
        m_pending = false;
        if (r.exitCode != 0) {
            const QString err = r.lastLine.isEmpty() ? i18nc("@info", "kdos-box could not list the boxes.") : r.lastLine;
            if (err != m_error) {
                m_error = err;
                Q_EMIT errorChanged();
            }
        } else {
            if (!m_error.isEmpty()) {
                m_error.clear();
                Q_EMIT errorChanged();
            }
            QList<BoxRow> rows = parseListing(r.output);
            for (BoxRow &row : rows) {
                decorate(row);
            }
            apply(std::move(rows));
        }
        if (!m_loaded) {
            m_loaded = true;
            Q_EMIT loadedChanged();
        }
        watchPaths();
        Q_EMIT refreshed();
        if (m_again) {
            m_again = false;
            refresh();
        }
    });
}

void BoxListModel::apply(QList<BoxRow> rows)
{
    std::sort(rows.begin(), rows.end(), [](const BoxRow &a, const BoxRow &b) {
        const int c = QString::localeAwareCompare(a.displayName, b.displayName);
        return c != 0 ? c < 0 : a.name < b.name;
    });

    bool same = rows.size() == m_rows.size();
    for (int i = 0; same && i < rows.size(); ++i) {
        same = rows.at(i).name == m_rows.at(i).name;
    }
    if (!same) {
        beginResetModel();
        m_rows = std::move(rows);
        endResetModel();
        Q_EMIT countChanged();
        return;
    }
    /* The same boxes in the same order: change rows in place, so a list a
     * person is scrolling or has selected in does not jump. */
    m_rows = std::move(rows);
    if (!m_rows.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(int(m_rows.size()) - 1));
    }
}

}
