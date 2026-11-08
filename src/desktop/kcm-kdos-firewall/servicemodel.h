/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   servicemodel — the named services, as the daemon reports them and as
 *                  the page would have them
 *
 * TWO STATES PER ROW: `applied` is what kdos-powerd said the last time it was
 * asked, `open` is what the switch shows. Apply sends exactly the rows where
 * the two differ, and Reset copies `applied` back over `open`. A fresh list
 * from the daemon replaces both, because after any change the daemon's
 * answer is the only one that is true.
 *
 * THE ROWS ARE THE DAEMON'S, in its order, and nothing here adds, renames or
 * reorders one: a second table of names on this side would be a second answer
 * to what `ssh` means.
 */

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QPair>

#include "firewalltypes.h"

namespace Kdos
{

class FirewallServiceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int openCount READ openCount NOTIFY pendingChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        OpenRole,
        AppliedRole,
        ChangedRole,
        KnocksRole,
    };
    Q_ENUM(Roles)

    using Change = QPair<QString, bool>;

    explicit FirewallServiceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    int openCount() const;

    void setServices(const FirewallServiceList &services);
    void setKnocks(const QHash<QString, quint64> &knocks);

    bool setOpen(int row, bool open);
    void revertPending();
    void closeAll();

    bool isDirty() const;
    bool pendingAllClosed() const;
    QList<Change> pendingChanges() const;

Q_SIGNALS:
    void countChanged();
    void pendingChanged();

private:
    struct Row {
        QString name;
        QString description;
        bool applied = false;
        bool open = false;
        quint64 knocks = 0;
    };
    QList<Row> m_rows;
};

} // namespace Kdos
