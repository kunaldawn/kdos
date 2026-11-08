/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   polkitcheck — would polkit let this process do it, asked without a prompt
 *
 * The page goes read-only when polkit's answer to io.kdos.firewall.set is
 * no. POLKIT IS ASKED, NOT THE GROUP FILE: the daemon decides by the same
 * question, so a second rule here ("is the user in wheel") would be a second
 * answer that drifts from the first the day 50-kdos.rules changes.
 *
 * The question is asked with no interaction, so it never raises a dialog:
 * yes, "yes after a password" (challenge) and no are the three answers, and
 * only no closes the page. The subject is this connection's unique bus name,
 * which is the subject the daemon's own check sees when the page calls it.
 */

#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

namespace Kdos
{

class PolkitCheck : public QObject
{
    Q_OBJECT

public:
    enum class Answer {
        Yes,
        Challenge,
        No,
        Unreachable,
    };
    Q_ENUM(Answer)

    PolkitCheck(const QDBusConnection &bus, const QString &actionId, QObject *parent = nullptr);

    void check();

Q_SIGNALS:
    void answered(Kdos::PolkitCheck::Answer answer);

private:
    QDBusConnection m_bus;
    QString m_actionId;
};

} // namespace Kdos
