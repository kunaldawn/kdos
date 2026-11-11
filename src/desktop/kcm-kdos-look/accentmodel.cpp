/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   accentmodel — the eight accents, each drawn in its own colours
 */

#include "accentmodel.h"

#include <KColorScheme>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QRegularExpression>
#include <QStandardPaths>

namespace Kdos
{

namespace
{
/* An accent name is what libkcolor spells one with. Anything else on a line
 * of the list is not an accent, and is never passed back to `kdos`. */
bool isAccentName(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^[a-z][a-z0-9-]{0,30}$"));
    return re.match(s).hasMatch();
}

QString capitalised(const QString &id)
{
    return id.isEmpty() ? id : id.left(1).toUpper() + id.mid(1);
}
}

AccentModel::AccentModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QString AccentModel::schemeName(const QString &accent)
{
    return QStringLiteral("KDOS") + capitalised(accent);
}

AccentModel::Row AccentModel::load(const QString &id)
{
    Row r;
    r.id = id;
    r.scheme = schemeName(id);
    r.label = capitalised(id);
    const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes/%1.colors").arg(r.scheme));
    if (path.isEmpty()) {
        return r;
    }
    const KSharedConfigPtr config = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
    const QString name = KConfigGroup(config, QStringLiteral("General")).readEntry("Name", QString());
    if (name.startsWith(QLatin1String("KDOS "))) {
        r.label = name.mid(5);
    }
    const KColorScheme window(QPalette::Active, KColorScheme::Window, config);
    const KColorScheme view(QPalette::Active, KColorScheme::View, config);
    r.window = window.background(KColorScheme::NormalBackground).color();
    r.text = window.foreground(KColorScheme::NormalText).color();
    r.view = view.background(KColorScheme::NormalBackground).color();
    r.accent = view.decoration(KColorScheme::FocusColor).color();
    r.available = true;
    return r;
}

int AccentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant AccentModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return r.label;
    case AccentIdRole:
        return r.id;
    case SchemeRole:
        return r.scheme;
    case CurrentRole:
        return r.current;
    case AvailableRole:
        return r.available;
    case WindowColorRole:
        return r.window;
    case ViewColorRole:
        return r.view;
    case TextColorRole:
        return r.text;
    case AccentColorRole:
        return r.accent;
    default:
        return {};
    }
}

QHash<int, QByteArray> AccentModel::roleNames() const
{
    return {
        {AccentIdRole, "accentId"},
        {LabelRole, "label"},
        {SchemeRole, "scheme"},
        {CurrentRole, "current"},
        {AvailableRole, "available"},
        {WindowColorRole, "windowColor"},
        {ViewColorRole, "viewColor"},
        {TextColorRole, "textColor"},
        {AccentColorRole, "accentColor"},
    };
}

int AccentModel::count() const
{
    return int(m_rows.size());
}

QString AccentModel::setFromList(const QString &listOutput)
{
    QList<Row> rows;
    QString current;
    const QStringList lines = listOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        const bool marked = line.startsWith(QLatin1Char('*'));
        if (marked) {
            line = line.mid(1).trimmed();
        }
        const QString id = line.section(QLatin1Char(' '), 0, 0, QString::SectionSkipEmpty);
        if (!isAccentName(id)) {
            continue;
        }
        Row r = load(id);
        r.current = marked;
        if (marked) {
            current = id;
        }
        rows.append(r);
    }
    const int before = int(m_rows.size());
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (before != m_rows.size()) {
        Q_EMIT countChanged();
    }
    return current;
}

bool AccentModel::contains(const QString &accent) const
{
    for (const Row &r : m_rows) {
        if (r.id == accent) {
            return true;
        }
    }
    return false;
}

bool AccentModel::isAvailable(const QString &accent) const
{
    for (const Row &r : m_rows) {
        if (r.id == accent) {
            return r.available;
        }
    }
    return false;
}

QString AccentModel::labelOf(const QString &accent) const
{
    for (const Row &r : m_rows) {
        if (r.id == accent) {
            return r.label;
        }
    }
    return capitalised(accent);
}

QString AccentModel::accentOfScheme(const QString &scheme) const
{
    for (const Row &r : m_rows) {
        if (r.scheme == scheme) {
            return r.id;
        }
    }
    return {};
}

} // namespace Kdos
