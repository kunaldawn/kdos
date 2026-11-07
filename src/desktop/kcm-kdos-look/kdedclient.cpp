/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdedclient — what kded-kdos's accent sync is doing
 */

#include "kdedclient.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QTimer>

namespace Kdos
{

namespace
{
/* Two short reads from a module in this session's own kded6. */
constexpr int CallTimeoutMs = 3000;
/* A run is a second or two; asking every few seconds while one is said to
 * be going costs nothing and ends a wait on a kded6 that went away. */
constexpr int RunningPollMs = 3000;
}

QString KdedClient::serviceName()
{
    return QStringLiteral("org.kde.kded6");
}

QString KdedClient::objectPath()
{
    return QStringLiteral("/modules/kdos");
}

QString KdedClient::interfaceName()
{
    return QStringLiteral("io.kdos.Kded1");
}

KdedClient::KdedClient(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_poll(new QTimer(this))
{
    m_poll->setInterval(RunningPollMs);
    connect(m_poll, &QTimer::timeout, this, &KdedClient::query);

    m_bus.connect(serviceName(), objectPath(), interfaceName(), QStringLiteral("accentSynced"), QStringLiteral("s"), this, SLOT(onSynced(QString)));
    m_bus.connect(serviceName(),
                  objectPath(),
                  interfaceName(),
                  QStringLiteral("accentSyncFailed"),
                  QStringLiteral("ss"),
                  this,
                  SLOT(onFailed(QString, QString)));
}

KdedClient::State KdedClient::state() const
{
    return m_state;
}

QString KdedClient::lastError() const
{
    return m_error;
}

void KdedClient::query()
{
    /* One question at a time: two out at once could answer in either order,
     * and the older answer land last. One asked meanwhile is asked again
     * after, because the answer in flight may predate what prompted it. */
    if (m_querying) {
        m_again = true;
        return;
    }
    m_querying = true;
    m_again = false;
    const quint64 asked = m_generation;
    QDBusMessage call = QDBusMessage::createMethodCall(serviceName(), objectPath(), interfaceName(), QStringLiteral("syncState"));
    /* Never started for this question: kded6 is the session's, and a page
     * that activated one would have started a second set of modules in a
     * session that runs without them. */
    call.setAutoStartService(false);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, CallTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, asked](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QString> reply = *w;
        const QString state = reply.isError() ? QString() : reply.value();
        if (state != QLatin1String("failed")) {
            answered(asked, reply.isError() ? State::Absent : state == QLatin1String("running") ? State::Running : State::Idle, {});
            return;
        }
        QDBusMessage why = QDBusMessage::createMethodCall(serviceName(), objectPath(), interfaceName(), QStringLiteral("lastError"));
        why.setAutoStartService(false);
        auto *second = new QDBusPendingCallWatcher(m_bus.asyncCall(why, CallTimeoutMs), this);
        connect(second, &QDBusPendingCallWatcher::finished, this, [this, asked](QDBusPendingCallWatcher *w2) {
            w2->deleteLater();
            const QDBusPendingReply<QString> error = *w2;
            answered(asked, State::Failed, error.isError() ? QString() : error.value());
        });
    });
}

void KdedClient::answered(quint64 asked, State state, const QString &error)
{
    m_querying = false;
    if (asked == m_generation) {
        setState(state, error);
    }
    if (m_again) {
        query();
    }
}

/* A run this client never saw start still ended, and what it wrote is still
 * new; setState() reports the end of one it did see. */
void KdedClient::onSynced(const QString &accent)
{
    Q_UNUSED(accent);
    ++m_generation;
    const bool seen = m_state == State::Running;
    setState(State::Idle, {});
    if (!seen) {
        Q_EMIT runEnded();
    }
}

void KdedClient::onFailed(const QString &accent, const QString &message)
{
    Q_UNUSED(accent);
    ++m_generation;
    const bool seen = m_state == State::Running;
    setState(State::Failed, message);
    if (!seen) {
        Q_EMIT runEnded();
    }
}

void KdedClient::setState(State state, const QString &error)
{
    if (state == State::Running) {
        m_poll->start();
    } else {
        m_poll->stop();
    }
    if (m_state == state && m_error == error) {
        return;
    }
    const bool ended = m_state == State::Running && state != State::Running;
    m_state = state;
    m_error = error;
    Q_EMIT stateChanged();
    /* A run that ended between two questions sent no signal this client
     * heard; its end is as much an end as one that did. */
    if (ended) {
        Q_EMIT runEnded();
    }
}

} // namespace Kdos
