/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   servicemodel — the named services, applied and pending
 */

#include "servicemodel.h"

namespace Kdos
{

FirewallServiceModel::FirewallServiceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FirewallServiceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant FirewallServiceModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return r.name;
    case DescriptionRole:
        return r.description;
    case OpenRole:
        return r.open;
    case AppliedRole:
        return r.applied;
    case ChangedRole:
        return r.open != r.applied;
    case KnocksRole:
        /* QML numbers are doubles; a count past 2^53 is not one a person
         * reads digit by digit. */
        return double(r.knocks);
    default:
        return {};
    }
}

QHash<int, QByteArray> FirewallServiceModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {OpenRole, "open"},
        {AppliedRole, "applied"},
        {ChangedRole, "changed"},
        {KnocksRole, "knocks"},
    };
}

int FirewallServiceModel::count() const
{
    return int(m_rows.size());
}

int FirewallServiceModel::openCount() const
{
    int n = 0;
    for (const Row &r : m_rows) {
        n += r.open ? 1 : 0;
    }
    return n;
}

void FirewallServiceModel::setServices(const FirewallServiceList &services)
{
    QHash<QString, quint64> knocks;
    for (const Row &r : std::as_const(m_rows)) {
        knocks.insert(r.name, r.knocks);
    }
    const int before = int(m_rows.size());
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(services.size());
    for (const FirewallService &s : services) {
        if (s.name.isEmpty()) {
            continue;
        }
        Row r;
        r.name = s.name;
        r.description = s.description;
        r.applied = s.open;
        r.open = s.open;
        /* A service that is now open turns nothing away; a closed one keeps
         * its count until the next knock list says otherwise. */
        r.knocks = s.open ? 0 : knocks.value(s.name);
        m_rows.append(r);
    }
    endResetModel();
    if (before != m_rows.size()) {
        Q_EMIT countChanged();
    }
    Q_EMIT pendingChanged();
}

void FirewallServiceModel::setKnocks(const QHash<QString, quint64> &knocks)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        Row &r = m_rows[i];
        const quint64 k = r.applied ? 0 : knocks.value(r.name);
        if (k != r.knocks) {
            r.knocks = k;
            const QModelIndex idx = index(i);
            Q_EMIT dataChanged(idx, idx, {KnocksRole});
        }
    }
}

bool FirewallServiceModel::setOpen(int row, bool open)
{
    if (row < 0 || row >= m_rows.size() || m_rows.at(row).open == open) {
        return false;
    }
    m_rows[row].open = open;
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, {OpenRole, ChangedRole});
    Q_EMIT pendingChanged();
    return true;
}

void FirewallServiceModel::revertPending()
{
    bool any = false;
    for (int i = 0; i < m_rows.size(); ++i) {
        Row &r = m_rows[i];
        if (r.open != r.applied) {
            r.open = r.applied;
            any = true;
            const QModelIndex idx = index(i);
            Q_EMIT dataChanged(idx, idx, {OpenRole, ChangedRole});
        }
    }
    if (any) {
        Q_EMIT pendingChanged();
    }
}

void FirewallServiceModel::closeAll()
{
    bool any = false;
    for (int i = 0; i < m_rows.size(); ++i) {
        Row &r = m_rows[i];
        if (r.open) {
            r.open = false;
            any = true;
            const QModelIndex idx = index(i);
            Q_EMIT dataChanged(idx, idx, {OpenRole, ChangedRole});
        }
    }
    if (any) {
        Q_EMIT pendingChanged();
    }
}

bool FirewallServiceModel::isDirty() const
{
    for (const Row &r : m_rows) {
        if (r.open != r.applied) {
            return true;
        }
    }
    return false;
}

bool FirewallServiceModel::pendingAllClosed() const
{
    return openCount() == 0;
}

QList<FirewallServiceModel::Change> FirewallServiceModel::pendingChanges() const
{
    QList<Change> out;
    for (const Row &r : m_rows) {
        if (r.open != r.applied) {
            out.append({r.name, r.open});
        }
    }
    return out;
}

} // namespace Kdos
