/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   firewallcontroller — load, apply and reset the named services
 *
 * The page's logic without the page: the KCM is a thin shell over this, so
 * the tests drive exactly what System Settings drives.
 *
 * APPLY IS ONE CALL PER CHANGED ROW, IN ORDER, AND STOPS AT THE FIRST
 * REFUSAL. The daemon rewrites the ruleset whole on every call, so there is no
 * batch to make atomic; what matters is that a refusal is not followed by
 * more changes the person did not see go through. After the last call, or
 * the refusal, the list is asked for again and replaces both states, so the
 * switches show what the kernel runs rather than what was asked for.
 *
 * READ-ONLY IS POLKIT'S ANSWER. The switches are disabled only when polkit
 * says no to io.kdos.firewall.set without interaction, or cannot be asked.
 * "Challenge" leaves them live: the password is asked for when a change is
 * applied, by the daemon's own check.
 *
 * A change made elsewhere — `kdos-power firewall`, kdos-fwask's Allow —
 * reloads the list at once unless the page holds unapplied changes, which a
 * reload would throw away under the person's hands.
 */

#pragma once

#include <QDBusConnection>
#include <QList>
#include <QObject>
#include <QString>

#include "polkitcheck.h"
#include "servicemodel.h"

namespace Kdos
{

class Power1Client;

class FirewallController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Kdos::FirewallServiceModel *services READ services CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(QString unavailableReason READ unavailableReason NOTIFY stateChanged)
    Q_PROPERTY(bool canChange READ canChange NOTIFY stateChanged)
    Q_PROPERTY(QString readOnlyReason READ readOnlyReason NOTIFY stateChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    static QString actionId();

    explicit FirewallController(const QDBusConnection &bus, QObject *parent = nullptr);

    FirewallServiceModel *services() const;
    bool loading() const;
    bool available() const;
    QString unavailableReason() const;
    bool canChange() const;
    QString readOnlyReason() const;
    bool applying() const;
    QString errorMessage() const;

    bool isDirty() const;
    bool representsDefaults() const;

    Q_INVOKABLE void load();
    void apply();
    void revert();
    void defaults();

    Q_INVOKABLE void setServiceOpen(int row, bool open);
    Q_INVOKABLE void dismissError();

Q_SIGNALS:
    void stateChanged();
    void errorMessageChanged();
    void dirtyChanged();
    /* After an apply has run to its end or to its first refusal, and the
     * fresh list is in. */
    void applied(bool ok);

private:
    void applyNext();
    void finishApply(bool ok);
    void setError(const QString &message);
    void onPolkit(PolkitCheck::Answer answer);

    Power1Client *m_client;
    PolkitCheck *m_polkit;
    FirewallServiceModel *m_model;

    bool m_listPending = false;
    bool m_polkitPending = false;
    bool m_available = false;
    QString m_unavailableReason;
    PolkitCheck::Answer m_answer = PolkitCheck::Answer::Unreachable;
    bool m_applying = false;
    bool m_applyOk = true;
    QList<FirewallServiceModel::Change> m_queue;
    QString m_error;
};

} // namespace Kdos
