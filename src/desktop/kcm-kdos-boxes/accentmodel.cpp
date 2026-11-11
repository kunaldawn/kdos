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

#include "accentmodel.h"

#include <KColorScheme>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace Kdos
{

AccentModel::AccentModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const QList<std::pair<QString, QString>> accents{
        {u"session"_s, i18nc("@item:inlistbox box colour", "Neutral")},
        {u"phosphor"_s, i18nc("@item:inlistbox accent", "Phosphor")},
        {u"amber"_s, i18nc("@item:inlistbox accent", "Amber")},
        {u"ice"_s, i18nc("@item:inlistbox accent", "Ice")},
        {u"bone"_s, i18nc("@item:inlistbox accent", "Bone")},
        {u"norton"_s, i18nc("@item:inlistbox accent", "Norton")},
        {u"borland"_s, i18nc("@item:inlistbox accent", "Borland")},
        {u"perfect"_s, i18nc("@item:inlistbox accent", "Perfect")},
        {u"paper"_s, i18nc("@item:inlistbox accent", "Paper")},
    };
    for (const auto &[id, label] : accents) {
        Accent a{id, label, {}, {}, {}};
        const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"color-schemes/"_s + schemeName(id) + u".colors"_s);
        if (!path.isEmpty()) {
            a.color = titleBarColor(path, &a.text, &a.accent);
        }
        m_accents << a;
    }
}

QString AccentModel::schemeName(const QString &id)
{
    if (id.isEmpty() || id == "session"_L1) {
        return u"KDOSBox"_s;
    }
    return u"KDOS"_s + id.left(1).toUpper() + id.mid(1);
}

QColor AccentModel::titleBarColor(const QString &schemePath, QColor *text, QColor *accent)
{
    KSharedConfigPtr cfg = KSharedConfig::openConfig(schemePath, KConfig::SimpleConfig);
    /* The Header set is what a Breeze title bar is drawn from when the
     * scheme has one; an older scheme's title bar is its [WM] colours. */
    if (cfg->hasGroup(u"Colors:Header"_s)) {
        const KColorScheme scheme(QPalette::Active, KColorScheme::Header, cfg);
        if (text) {
            *text = scheme.foreground(KColorScheme::NormalText).color();
        }
        if (accent) {
            *accent = scheme.decoration(KColorScheme::FocusColor).color();
        }
        return scheme.background(KColorScheme::NormalBackground).color();
    }
    const KConfigGroup wm(cfg, u"WM"_s);
    if (text) {
        *text = wm.readEntry("activeForeground", QColor());
    }
    if (accent) {
        *accent = wm.readEntry("activeBlend", QColor());
    }
    return wm.readEntry("activeBackground", QColor());
}

int AccentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_accents.size());
}

QVariant AccentModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const Accent &a = m_accents.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return a.label;
    case IdRole:
        return a.id;
    case ColorRole:
        return a.color;
    case TextColorRole:
        return a.text;
    case AccentColorRole:
        return a.accent;
    case AvailableRole:
        return a.color.isValid();
    case SchemeRole:
        return schemeName(a.id);
    }
    return {};
}

QHash<int, QByteArray> AccentModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {IdRole, "accentId"},
        {LabelRole, "label"},
        {ColorRole, "swatchColor"},
        {TextColorRole, "swatchTextColor"},
        {AccentColorRole, "swatchAccentColor"},
        {AvailableRole, "available"},
        {SchemeRole, "scheme"},
    };
}

int AccentModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_accents.size(); ++i) {
        if (m_accents.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

QString AccentModel::labelOf(const QString &id) const
{
    const int i = indexOf(id);
    return i < 0 ? id : m_accents.at(i).label;
}

}
