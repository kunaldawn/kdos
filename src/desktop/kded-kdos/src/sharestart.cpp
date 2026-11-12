/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — Samba started for a session that can share folders
 * ---------------------------------
 */

#include "sharestart.h"
#include "debug.h"

#include <KLocalizedString>
#include <KNotification>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>

#include <unistd.h>

namespace Kdos
{

/* The `usershare path` of the shipped /etc/samba/smb.conf. */
static const QString USERSHARES = QStringLiteral("/var/lib/samba/usershares");

/* polkit may ask for a password under a stricter rule than the shipped
 * one, and a person takes as long as they take. */
static constexpr int CALL_TIMEOUT_MS = 130 * 1000;

ShareStart::ShareStart(QObject *parent)
    : QObject(parent)
{
    /* What `net usershare add` needs of this session's credentials: to make
     * a file in the directory. */
    if (::access(QFile::encodeName(USERSHARES).constData(), W_OK | X_OK) != 0) {
        qCInfo(KDOS_KDED) << "not starting Samba:" << USERSHARES
                          << "is not writable - Samba is not installed, or this session is not in sambashare";
        return;
    }
    start();
}

void ShareStart::start()
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("io.kdos.Power1"),
                                                       QStringLiteral("/io/kdos/Power1"),
                                                       QStringLiteral("io.kdos.Power1"),
                                                       QStringLiteral("StartFileSharing"));

    call.setInteractiveAuthorizationAllowed(true);

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call, CALL_TIMEOUT_MS),
                                                this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<> reply = *w;

        w->deleteLater();
        if (reply.isError()) {
            const QString message = reply.error().message().left(200);

            qCWarning(KDOS_KDED) << "Samba was not started for this session:" << reply.error().name()
                                 << message;
            notifyFailure(message);
            return;
        }
        qCInfo(KDOS_KDED) << "this session can share folders; Samba is running";
    });
}

void ShareStart::notifyFailure(const QString &message)
{
    /* Once per reason in a session: every share would fail the same way,
     * for the same thing to fix. */
    if (m_reported.contains(message)) {
        return;
    }
    m_reported.insert(message);

    auto *n = new KNotification(QStringLiteral("shareStartFailed"));

    n->setComponentName(QStringLiteral("kded_kdos"));
    n->setTitle(i18nc("@title", "Folder sharing unavailable"));
    n->setText(i18nc("@info %1 is the reason",
                     "The file server could not be started, so folders cannot be shared: %1",
                     message.toHtmlEscaped()));
    n->setIconName(QStringLiteral("folder-remote"));
    n->sendEvent();
}

}
