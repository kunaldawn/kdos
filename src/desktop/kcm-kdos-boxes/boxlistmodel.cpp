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
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

}

bool BoxRow::isRunning() const
{
    return state == "running"_L1;
}

bool BoxRow::hasContainer() const
{
    return !state.isEmpty() && state != "not created"_L1 && state != "absent"_L1;
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
        return stateText(r.state);
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
        QStringList parts{stateText(r.state)};
        if (r.isRunning() && !r.confinement.isEmpty()) {
            parts << confinementText(r.confinement);
        }
        if (r.displayName != r.name) {
            parts << r.name;
        }
        return parts.join(u" · "_s);
    }
    case ExportIdRole:
        return r.exportId();
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

QString BoxListModel::stateText(const QString &state)
{
    if (state == "running"_L1) {
        return i18nc("@info box state", "Running");
    }
    if (state == "not created"_L1 || state.isEmpty() || state == "absent"_L1) {
        return i18nc("@info box state", "Not created yet");
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
        rows << r;
    }
    return rows;
}

void BoxListModel::decorate(BoxRow &row) const
{
    row.displayName = row.name;
    row.iconName = u"preferences-security"_s;

    KService::Ptr service = KService::serviceByDesktopName(row.name);
    if (!service) {
        const QString box = row.name;
        const KService::List found = KApplicationTrader::query([&box](const KService::Ptr &s) {
            return s->property<QString>(u"X-KDOS-Box"_s) == box;
        });
        if (!found.isEmpty()) {
            service = found.first();
        }
    }
    if (service) {
        if (!service->name().isEmpty()) {
            row.displayName = service->name();
        }
        if (!service->icon().isEmpty()) {
            row.iconName = service->icon();
        }
    }
    if (row.isRunning()) {
        const ConfinementRecord rec = readConfinementRecord(row.name);
        row.confinement = confinementId(rec.state);
        row.confinementReason = rec.state == Confinement::Confined ? QString() : rec.reason;
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
