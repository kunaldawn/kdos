/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   accentmodel — the eight accents, each drawn in its own colours
 *
 * THE LIST IS `kdos theme list`'s, in its order. libkcolor holds the one
 * table of accents and kdos-tools prints it; a second list here would be an
 * accent the command refuses, or one the page never offers.
 *
 * A SWATCH IS PAINTED FROM ITS COLOUR SCHEME, never from a literal: each row
 * reads /usr/share/color-schemes/KDOS<Label>.colors through KColorScheme and
 * exposes that scheme's own roles (window, view, text, and the accent itself
 * as the view's focus decoration), so the swatch is the scheme a click
 * applies. The accent is read from DecorationFocus, not the selection: a
 * dark accent's selection is the accent darkened until the view's text reads
 * on it, and a band painted from it would show a muddier colour than the
 * accent a click applies. An accent whose scheme file is not
 * installed is listed and cannot be picked: applying it would leave Plasma
 * on the previous colours while every other surface moved.
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
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        AccentIdRole = Qt::UserRole + 1,
        LabelRole,
        SchemeRole,
        CurrentRole,
        AvailableRole,
        WindowColorRole,
        ViewColorRole,
        TextColorRole,
        AccentColorRole,
    };
    Q_ENUM(Roles)

    explicit AccentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    /* `kdos theme list`'s output: one accent per line, the first word its
     * name, a leading `*` on the one in force. Returns that one, or "". */
    QString setFromList(const QString &listOutput);

    bool contains(const QString &accent) const;
    bool isAvailable(const QString &accent) const;
    QString labelOf(const QString &accent) const;
    /* The accent whose scheme is called this, or "" for a scheme that is
     * not one of KDOS's (Breeze, a downloaded one, a hand-made one). */
    QString accentOfScheme(const QString &scheme) const;

    /* The scheme an accent applies: KDOS<Label>, the file KDOS<Label>.colors. */
    static QString schemeName(const QString &accent);

Q_SIGNALS:
    void countChanged();

private:
    struct Row {
        QString id;
        QString label;
        QString scheme;
        bool current = false;
        bool available = false;
        QColor window;
        QColor view;
        QColor text;
        QColor accent;
    };
    static Row load(const QString &id);
    QList<Row> m_rows;
};

} // namespace Kdos
