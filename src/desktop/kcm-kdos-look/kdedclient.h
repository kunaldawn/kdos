/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdedclient — what kded-kdos's accent sync is doing
 *
 * kded-kdos carries an accent picked on the Global Theme or Colors page down
 * to the terminals and the screens before login, by running `kdos theme
 * --from-plasma <accent>` itself. Its state is on the session bus at
 * org.kde.kded6 /modules/kdos, interface io.kdos.Kded1: syncState() is
 * "idle", "running" or "failed", lastError() the reason of the last failure,
 * and accentSynced(s) / accentSyncFailed(s, s) mark the end of each run.
 *
 * A RUN THAT HAS STARTED SENDS NO SIGNAL; only its end does. So the state is
 * asked for whenever the page has a reason to think a run began (a changed
 * colour scheme), and asked again every few seconds while it says running,
 * so a kded6 that went away mid-run does not leave the page waiting on a run
 * nobody is doing.
 *
 * THE BUS IS A PARAMETER: the page uses the session bus, the tests serve a
 * fake module on a private one.
 */

#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

class QTimer;

namespace Kdos
{

class KdedClient : public QObject
{
    Q_OBJECT

public:
    enum class State {
        /* No kded6, or no kdos module in it: nothing carries a pick made
         * on another page, and the page's own action is the only one. */
        Absent,
        Idle,
        Running,
        Failed,
    };
    Q_ENUM(State)

    static QString serviceName();
    static QString objectPath();
    static QString interfaceName();

    explicit KdedClient(const QDBusConnection &bus, QObject *parent = nullptr);

    State state() const;
    /* The last run's reason, while the state is Failed. */
    QString lastError() const;

    /* Ask the module where it is now. */
    void query();

Q_SIGNALS:
    void stateChanged();
    /* A run ended, either way; what it wrote is worth reading again. */
    void runEnded();

private Q_SLOTS:
    void onSynced(const QString &accent);
    void onFailed(const QString &accent, const QString &message);

private:
    void answered(quint64 asked, State state, const QString &error);
    void setState(State state, const QString &error);

    QDBusConnection m_bus;
    QTimer *m_poll;
    State m_state = State::Absent;
    QString m_error;
    bool m_querying = false;
    bool m_again = false;
    /* Bumped by each signal: an answer to a question asked before the
     * signal describes a moment the signal has already superseded. */
    quint64 m_generation = 0;
};

} // namespace Kdos
