/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   doctormodel — `kdos doctor --json`, a row per check
 *
 * The report is {"checks": [{"section", "level", "message"}...],
 * "warnings": N}, the records doctor's own ok/warn/skip funnels write, so
 * the page shows exactly what a person running `kdos doctor` is told and in
 * the same order. A level the page does not know is shown as a warning: a
 * check that reported something new is a check worth reading.
 */

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QSortFilterProxyModel>
#include <QString>

namespace Kdos
{

class DoctorModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Level {
        Ok,
        Warn,
        Skip,
    };
    Q_ENUM(Level)

    enum Roles {
        SectionRole = Qt::UserRole + 1,
        LevelRole,
        MessageRole,
    };
    Q_ENUM(Roles)

    explicit DoctorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /* Replaces the rows. False, leaving none, when this is not a report. */
    bool parse(const QByteArray &json);
    void clear();

    int count(Level level) const;
    /* The report as `kdos doctor` prints it, for the clipboard. */
    QString text() const;

private:
    struct Row {
        QString section;
        Level level = Ok;
        QString message;
    };
    QList<Row> m_rows;
};

/* Rows whose message or section holds the filter, ignoring case. */
class DoctorFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit DoctorFilterModel(QObject *parent = nullptr);

    void setFilterText(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_text;
};

} // namespace Kdos
