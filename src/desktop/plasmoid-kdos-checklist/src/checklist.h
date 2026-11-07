/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   checklist — the first-run checklist that ticks itself
 *
 * `kdos checklist --json` says which items this machine has and whether each
 * is done or dismissed:
 *
 *   {"version":1,"items":[{"id":"backup","done":false,"dismissed":false},…]}
 *
 * The words and what each item opens live here, keyed by id, so they are
 * translated with the rest of the desktop and nothing the probe prints is
 * ever run. An id this table does not know is left out.
 */

#pragma once

#include <QAbstractListModel>
#include <QProcess>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

namespace Kdos
{

struct ChecklistEntry {
    QString id;
    bool done = false;
    bool dismissed = false;
};

struct ChecklistText {
    QString title;
    QString explanation;
    QString actionText;
    QString icon;
    bool optional = false;
    /* What "do it" runs: a program and its arguments, never a command line. */
    QStringList command;
};

/* One run of the probe. Not a list of version 1 → an empty list and false. */
bool kdosParseChecklist(const QByteArray &json, QList<ChecklistEntry> *out);

/* The words and the command for an item; an empty title for an unknown id. */
ChecklistText kdosChecklistText(const QString &id);

/* The ids the checklist knows, in the order it shows them. */
QStringList kdosChecklistIds();

class ChecklistModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int remaining READ remaining NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    /* Every item done or dismissed; false until the probe has answered. */
    Q_PROPERTY(bool finished READ finished NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    /* Seconds between probes while an item is open; 0 runs it only on
     * refresh(). Dismissed and done items are not probed for again once
     * everything is finished. */
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ExplanationRole,
        ActionTextRole,
        IconRole,
        DoneRole,
        DismissedRole,
        OptionalRole,
    };
    Q_ENUM(Roles)

    explicit ChecklistModel(QObject *parent = nullptr);
    ~ChecklistModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int remaining() const;
    int total() const;
    bool finished() const;
    bool ready() const;
    QString problem() const;
    int interval() const;
    void setInterval(int seconds);

    Q_INVOKABLE void refresh();
    /* Runs the item's command; false when it cannot be started. */
    Q_INVOKABLE bool open(const QString &id);
    Q_INVOKABLE void dismiss(const QString &id);
    /* Brings every dismissed item back. */
    Q_INVOKABLE void restore();

    /* Where kdos is: the session's PATH, then /usr/local/bin. */
    static QString kdosProgram();

Q_SIGNALS:
    void changed();
    void intervalChanged();

private:
    void probeFinished(int code, QProcess::ExitStatus status);
    void apply(const QList<ChecklistEntry> &entries);
    void runKdos(const QStringList &arguments);
    void schedule();

    QList<ChecklistEntry> m_entries;
    QProcess *m_probe = nullptr;
    bool m_again = false;
    bool m_ready = false;
    QString m_problem;
    int m_interval = 5;
    QTimer m_timer;
};

} // namespace Kdos
