/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   keyboardmodel — the XKB layouts and their variants, out of xkeyboard-config
 */

#include "keyboardmodel.h"

#include <KLocalizedString>

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace Kdos
{

KeyboardModel::KeyboardModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

bool KeyboardModel::load(const QString &path)
{
    QString p = path;
    if (p.isEmpty()) {
        p = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("X11/xkb/rules/evdev.lst"));
    }
    if (p.isEmpty()) {
        p = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("X11/xkb/rules/base.lst"));
    }
    QFile f(p);
    if (p.isEmpty() || !f.open(QIODevice::ReadOnly)) {
        parse(QByteArrayLiteral("! layout\n  us              English (US)\n"));
        return false;
    }
    /* <xkb>/rules/evdev.lst beside <xkb>/symbols. */
    const QDir rules = QFileInfo(p).absoluteDir();
    const QString symbols = rules.absoluteFilePath(QStringLiteral("../symbols"));
    parse(f.readAll(), QDir(symbols).exists() ? QDir::cleanPath(symbols) : QString());
    return true;
}

/*
 * THE LIST'S TWO SECTIONS THAT MATTER: `! layout`, a name then its
 * description, and `! variant`, a name then `<layout>: <description>`. Every
 * other section (models, options) is skipped. Layouts are sorted by
 * description, which is what a person reads.
 */
void KeyboardModel::parse(const QByteArray &lst, const QString &symbolsDir)
{
    static const QRegularExpression row(QStringLiteral("^\\s+(\\S+)\\s+(.*)$"));
    QList<Layout> layouts;
    QHash<QString, QStringList> variants;
    QString section;

    for (const QByteArray &raw : lst.split('\n')) {
        const QString l = QString::fromUtf8(raw);
        if (l.startsWith(QLatin1Char('!'))) {
            section = l.mid(1).trimmed();
            continue;
        }
        const QRegularExpressionMatch m = row.match(l);
        if (!m.hasMatch()) {
            continue;
        }
        if (section == QLatin1String("layout")) {
            /* A layout with no symbols file fails to compile at login, and
             * the session falls back to US. */
            if (!symbolsDir.isEmpty() && !QFile::exists(symbolsDir + QLatin1Char('/') + m.captured(1))) {
                continue;
            }
            layouts.append({m.captured(1), m.captured(2).trimmed(), {}});
        } else if (section == QLatin1String("variant")) {
            const QString rest = m.captured(2);
            const qsizetype colon = rest.indexOf(QLatin1Char(':'));
            if (colon > 0) {
                variants[rest.left(colon).trimmed()] << m.captured(1) << rest.mid(colon + 1).trimmed();
            }
        }
    }
    for (Layout &lay : layouts) {
        lay.variants = QStringList{QString(), i18nc("@item:inlistbox the layout with no variant", "Default")};
        lay.variants += variants.value(lay.name);
    }
    QCollator c;
    c.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(layouts.begin(), layouts.end(), [&c](const Layout &a, const Layout &b) {
        return c.compare(a.description, b.description) < 0;
    });

    beginResetModel();
    m_layouts = layouts;
    endResetModel();
    Q_EMIT countChanged();
}

int KeyboardModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_layouts.size());
}

QVariant KeyboardModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Layout &l = m_layouts.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case DescriptionRole:
        return l.description;
    case LayoutRole:
        return l.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> KeyboardModel::roleNames() const
{
    return {
        {LayoutRole, "layout"},
        {DescriptionRole, "description"},
    };
}

int KeyboardModel::indexOf(const QString &layout) const
{
    for (int i = 0; i < m_layouts.size(); i++) {
        if (m_layouts.at(i).name == layout) {
            return i;
        }
    }
    return -1;
}

QString KeyboardModel::description(const QString &layout) const
{
    const int i = indexOf(layout);
    return i < 0 ? layout : m_layouts.at(i).description;
}

QStringList KeyboardModel::variants(const QString &layout) const
{
    const int i = indexOf(layout);
    return i < 0 ? QStringList{QString(), i18nc("@item:inlistbox the layout with no variant", "Default")} : m_layouts.at(i).variants;
}

QString KeyboardModel::variantDescription(const QString &layout, const QString &variant) const
{
    if (variant.isEmpty()) {
        return QString();
    }
    const QStringList v = variants(layout);
    for (qsizetype i = 0; i + 1 < v.size(); i += 2) {
        if (v.at(i) == variant) {
            return v.at(i + 1);
        }
    }
    return variant;
}

}
