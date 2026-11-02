/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   firewallcontroller — load, apply and reset the named services
 */

#include "firewallcontroller.h"

#include "power1client.h"

#include <KLocalizedString>

namespace Kdos
{

QString FirewallController::actionId()
{
    return QStringLiteral("io.kdos.firewall.set");
}

FirewallController::FirewallController(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_client(new Power1Client(bus, this))
    , m_polkit(new PolkitCheck(bus, actionId(), this))
    , m_model(new FirewallServiceModel(this))
{
    connect(m_model, &FirewallServiceModel::pendingChanged, this, &FirewallController::dirtyChanged);

    connect(m_client, &Power1Client::servicesListed, this, [this](const FirewallServiceList &services) {
        m_listPending = false;
        m_available = true;
        m_unavailableReason.clear();
        m_model->setServices(services);
        if (m_applying) {
            m_applying = false;
            Q_EMIT stateChanged();
            Q_EMIT applied(m_applyOk);
            return;
        }
        Q_EMIT stateChanged();
    });
    connect(m_client, &Power1Client::listFailed, this, [this](const QString &message) {
        m_listPending = false;
        m_available = false;
        m_unavailableReason = message;
        /* No list means no switch can be shown truthfully; the rows from an
         * earlier answer would be a firewall the page can no longer see. */
        m_model->setServices({});
        if (m_applying) {
            m_applying = false;
            Q_EMIT stateChanged();
            Q_EMIT applied(false);
            return;
        }
        Q_EMIT stateChanged();
    });
    connect(m_client, &Power1Client::knocksListed, m_model, &FirewallServiceModel::setKnocks);
    connect(m_client, &Power1Client::knocksUnavailable, this, [this] {
        m_model->setKnocks({});
    });
    connect(m_client, &Power1Client::serviceSet, this, [this] {
        applyNext();
    });
    connect(m_client, &Power1Client::setFailed, this, [this](const QString &name, const QString &message) {
        setError(i18nc("@info %1 is a firewall service name, %2 the reason", "“%1” was not changed. %2", name, message));
        m_queue.clear();
        finishApply(false);
    });
    connect(m_client, &Power1Client::changedElsewhere, this, [this] {
        if (!m_applying && !loading() && !m_model->isDirty()) {
            load();
        }
    });
    connect(m_polkit, &PolkitCheck::answered, this, &FirewallController::onPolkit);
}

FirewallServiceModel *FirewallController::services() const
{
    return m_model;
}

bool FirewallController::loading() const
{
    return m_listPending || m_polkitPending;
}

bool FirewallController::available() const
{
    return m_available;
}

QString FirewallController::unavailableReason() const
{
    return m_unavailableReason;
}

bool FirewallController::canChange() const
{
    return m_available && !m_polkitPending && (m_answer == PolkitCheck::Answer::Yes || m_answer == PolkitCheck::Answer::Challenge);
}

QString FirewallController::readOnlyReason() const
{
    if (m_polkitPending || !m_available) {
        return {};
    }
    switch (m_answer) {
    case PolkitCheck::Answer::No:
        return i18nc("@info", "Only an administrator can choose which services answer the network.");
    case PolkitCheck::Answer::Unreachable:
        return i18nc("@info", "polkit is not answering, so no change to the firewall could be authorized.");
    default:
        return {};
    }
}

bool FirewallController::applying() const
{
    return m_applying;
}

QString FirewallController::errorMessage() const
{
    return m_error;
}

bool FirewallController::isDirty() const
{
    return m_model->isDirty();
}

bool FirewallController::representsDefaults() const
{
    return m_model->pendingAllClosed();
}

void FirewallController::load()
{
    /* An apply ends by asking for the list itself; a second list answering
     * mid-apply would be taken for that end while changes are still going
     * out, and the page would report them applied before they were. */
    if (m_applying) {
        return;
    }
    m_listPending = true;
    m_polkitPending = true;
    Q_EMIT stateChanged();
    m_client->listServices();
    m_client->listKnocks();
    m_polkit->check();
}

void FirewallController::apply()
{
    if (m_applying || loading() || !canChange()) {
        return;
    }
    m_queue = m_model->pendingChanges();
    if (m_queue.isEmpty()) {
        Q_EMIT applied(true);
        return;
    }
    setError({});
    m_applying = true;
    m_applyOk = true;
    Q_EMIT stateChanged();
    applyNext();
}

void FirewallController::applyNext()
{
    if (m_queue.isEmpty()) {
        finishApply(m_applyOk);
        return;
    }
    const FirewallServiceModel::Change next = m_queue.takeFirst();
    m_client->setService(next.first, next.second);
}

void FirewallController::finishApply(bool ok)
{
    m_applyOk = ok;
    m_client->listServices();
    m_client->listKnocks();
}

void FirewallController::revert()
{
    m_model->revertPending();
}

void FirewallController::defaults()
{
    m_model->closeAll();
}

void FirewallController::setServiceOpen(int row, bool open)
{
    if (!canChange() || m_applying) {
        return;
    }
    m_model->setOpen(row, open);
}

void FirewallController::dismissError()
{
    setError({});
}

void FirewallController::setError(const QString &message)
{
    if (m_error == message) {
        return;
    }
    m_error = message;
    Q_EMIT errorMessageChanged();
}

void FirewallController::onPolkit(PolkitCheck::Answer answer)
{
    m_polkitPending = false;
    m_answer = answer;
    Q_EMIT stateChanged();
}

} // namespace Kdos
