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
 *
 * The colours a box can wear: its security domain's colour.
 *
 * A box's `accent` is the colour scheme KWin forces on its title bar
 * (decocolor=KDOS<Accent>, the rule genlaunchers writes), so a boxed window
 * and the same program on the host never look alike. `session` is no accent
 * of its own: such a box wears the neutral KDOSBox scheme, which is still not
 * the host's.
 *
 * THE SWATCH IS THE SCHEME'S OWN TITLE-BAR COLOUR, read from the installed
 * .colors file through KColorScheme, never a literal here: a swatch that
 * disagreed with what KWin paints would be a promise the window breaks. A
 * scheme that is not installed has no swatch, and the page says so. Most
 * KDOS title bars are near-black, so the swatch also carries the title's
 * colour and the scheme's accent (its focus decoration), which is what
 * tells one scheme from another.
 *
 * The eight names are libkcolor's KCOL_SCHEMES, in its order.
 */

#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QList>
#include <QString>

namespace Kdos
{

class AccentModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        LabelRole,
        ColorRole,
        TextColorRole,
        AccentColorRole,
        AvailableRole,
        SchemeRole,
    };
    Q_ENUM(Role)

    explicit AccentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int indexOf(const QString &id) const;
    Q_INVOKABLE QString labelOf(const QString &id) const;

    /* KDOS<Accent>, or KDOSBox for `session`. */
    static QString schemeName(const QString &id);
    /* The title-bar colours of an installed scheme file: its background,
     * and through `text` and `accent` its title and its focus decoration;
     * invalid when the scheme is not installed. Public for the tests. */
    static QColor titleBarColor(const QString &schemePath, QColor *text = nullptr, QColor *accent = nullptr);

private:
    struct Accent {
        QString id;
        QString label;
        QColor color;
        QColor text;
        QColor accent;
    };
    QList<Accent> m_accents;
};

}
