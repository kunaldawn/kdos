/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   keyboardmodel — the XKB layouts and their variants, out of xkeyboard-config
 *
 * Read from the rules list xkeyboard-config installs (evdev.lst), which is
 * what KWin compiles a layout against: a layout offered here is one the
 * installed session can load. A layout KWin cannot compile is not an error
 * anyone sees — it falls back to US with the password field included — so
 * nothing else is offered.
 */

#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace Kdos
{

class KeyboardModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The keyboard list belongs to the Backend")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        LayoutRole = Qt::UserRole + 1,
        DescriptionRole,
    };
    Q_ENUM(Roles)

    explicit KeyboardModel(QObject *parent = nullptr);

    /* Reads `path`, or the first rules list found where it is empty. */
    bool load(const QString &path = QString());
    /* The parser alone, for a test with a fixture list. */
    void parse(const QByteArray &lst);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int indexOf(const QString &layout) const;
    Q_INVOKABLE QString description(const QString &layout) const;
    /* A layout's variants: alternating name and description, the default
     * ("") first. */
    Q_INVOKABLE QStringList variants(const QString &layout) const;

Q_SIGNALS:
    void countChanged();

private:
    struct Layout {
        QString name, description;
        QStringList variants;
    };
    QList<Layout> m_layouts;
};

}
