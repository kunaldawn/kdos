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
 * The boxes, as `kdos-box list` reports them: every container podman knows
 * and every profile with no container yet, which is still a box, described
 * and not created.
 *
 * THE JSON LISTING ONLY. `kdos-box list --json` is the listing written for
 * this page; the table beside it is for a person and is never parsed, since
 * its STATE column can be two words and its widths follow the names.
 *
 * A BOX IS SHOWN UNDER ITS APPLICATION'S NAME AND ICON. genlaunchers writes
 * each launcher as `<box>.desktop` (or carries `X-KDOS-Box=<box>`), which is
 * the id KWin gives the box's windows, so the page and the task manager name
 * the same thing the same way.
 */

#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>

class QFileSystemWatcher;
class QTimer;

namespace Kdos
{

class BoxTool;

struct BoxRow {
    QString name;
    QString displayName;
    QString iconName;
    QString base;
    QString state;          /* podman's word, or "not created" */
    QString persistence;
    QString accent;
    qint64 diskBytes = 0;   /* what its container holds; 0 when it has none */
    QDateTime created;      /* invalid for a box with no container */
    QString confinement;    /* confined | tagged | unconfined | "" */
    QString confinementReason; /* the launch's own sentence, "" if none */

    bool isRunning() const;
    bool hasContainer() const;
    QString exportId() const;
    /* The size on disk for a person, "" when it holds nothing. */
    QString diskText() const;
};

class BoxListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        BaseRole,
        StateRole,
        StateTextRole,
        RunningRole,
        CreatedRole,
        PersistenceRole,
        AccentRole,
        DiskRole,
        ConfinementRole,
        ConfinementTextRole,
        ConfinementReasonRole,
        SubtitleRole,
        ExportIdRole,
    };
    Q_ENUM(Role)

    explicit BoxListModel(BoxTool *tool, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isLoaded() const;
    QString error() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE int indexOf(const QString &name) const;
    const BoxRow *row(const QString &name) const;

    /* Public for the tests: the rows a listing becomes. */
    static QList<BoxRow> parseListing(const QString &output);
    static QString stateText(const QString &state);
    static QString confinementText(const QString &confinement);

Q_SIGNALS:
    void loadedChanged();
    void errorChanged();
    void countChanged();
    /* After every successful refresh, for the page that shows one box. */
    void refreshed();

private:
    void apply(QList<BoxRow> rows);
    void decorate(BoxRow &row) const;
    void watchPaths();

    BoxTool *m_tool;
    QList<BoxRow> m_rows;
    bool m_loaded = false;
    bool m_pending = false;
    bool m_again = false;
    QString m_error;
    QFileSystemWatcher *m_watcher;
    QTimer *m_debounce;
    QTimer *m_poll;
};

}
