/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   doctormodel — `kdos doctor --json`, a row per check
 */

#include "doctormodel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Kdos
{

DoctorModel::DoctorModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DoctorModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant DoctorModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case MessageRole:
        return r.message;
    case SectionRole:
        return r.section;
    case LevelRole:
        /* A word, not a number, so the page reads as what it means. */
        return r.level == Ok ? QStringLiteral("ok") : r.level == Skip ? QStringLiteral("skip") : QStringLiteral("warn");
    default:
        return {};
    }
}

QHash<int, QByteArray> DoctorModel::roleNames() const
{
    return {
        {SectionRole, "section"},
        {LevelRole, "level"},
        {MessageRole, "message"},
    };
}

bool DoctorModel::parse(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    const QJsonValue checks = doc.object().value(QStringLiteral("checks"));
    QList<Row> rows;
    const bool ok = error.error == QJsonParseError::NoError && doc.isObject() && checks.isArray();
    if (ok) {
        const QJsonArray list = checks.toArray();
        for (const QJsonValue &v : list) {
            const QJsonObject o = v.toObject();
            Row r;
            r.section = o.value(QStringLiteral("section")).toString();
            r.message = o.value(QStringLiteral("message")).toString();
            const QString level = o.value(QStringLiteral("level")).toString();
            r.level = level == QLatin1String("ok") ? Ok : level == QLatin1String("skip") ? Skip : Warn;
            if (r.message.isEmpty()) {
                continue;
            }
            rows.append(r);
        }
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
    return ok;
}

void DoctorModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

int DoctorModel::count(Level level) const
{
    int n = 0;
    for (const Row &r : m_rows) {
        n += r.level == level ? 1 : 0;
    }
    return n;
}

QString DoctorModel::text() const
{
    QString out = QStringLiteral("KDOS doctor\n");
    QString section;
    bool first = true;
    for (const Row &r : m_rows) {
        if (first || r.section != section) {
            section = r.section;
            out += QLatin1Char('\n') + section + QLatin1Char('\n');
            first = false;
        }
        const char *tag = r.level == Ok ? "[ ok ]" : r.level == Skip ? "[skip]" : "[warn]";
        out += QStringLiteral("  ") + QLatin1String(tag) + QLatin1Char(' ') + r.message + QLatin1Char('\n');
    }
    return out;
}

DoctorFilterModel::DoctorFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void DoctorFilterModel::setFilterText(const QString &text)
{
    if (m_text == text) {
        return;
    }
    beginFilterChange();
    m_text = text;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool DoctorFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_text.isEmpty()) {
        return true;
    }
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    return idx.data(DoctorModel::MessageRole).toString().contains(m_text, Qt::CaseInsensitive)
        || idx.data(DoctorModel::SectionRole).toString().contains(m_text, Qt::CaseInsensitive);
}

} // namespace Kdos
