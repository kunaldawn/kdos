/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — Samba started for the first shared folder
 * ---------------------------------
 */

#include "sharewatch.h"
#include "debug.h"
#include "logic.h"

#include <KLocalizedString>
#include <KNotification>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace Kdos
{

/* The `usershare path` of the shipped /etc/samba/smb.conf. */
static const QString USERSHARES = QStringLiteral("/var/lib/samba/usershares");

/* `net usershare add` writes a temporary file and renames it; the directory
 * is looked at once the burst has settled. */
static constexpr int DEBOUNCE_MS = 500;

/* Directory entries looked at, at most: a hundred shares is smb.conf's
 * `usershare max shares`. */
static constexpr int SCAN_LIMIT = 1024;

/* polkit may ask for a password under a stricter rule than the shipped
 * one, and a person takes as long as they take. */
static constexpr int CALL_TIMEOUT_MS = 130 * 1000;

ShareWatch::ShareWatch(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(DEBOUNCE_MS);
    connect(&m_debounce, &QTimer::timeout, this, &ShareWatch::evaluate);

    if (!m_watcher.addPath(USERSHARES)) {
        qCInfo(KDOS_KDED) << "not watching" << USERSHARES
                          << "- Samba is not installed, or this session is not in sambashare";
        return;
    }
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        m_debounce.start();
    });

    /* What is there at login was there at boot, when Samba's service script
     * made its own decision about it; only a change from here on asks. */
    m_shared = hasUsershare(USERSHARES, SCAN_LIMIT);
}

void ShareWatch::evaluate()
{
    const bool shared = hasUsershare(USERSHARES, SCAN_LIMIT);
    const bool first = shared && !m_shared;

    m_shared = shared;
    if (first) {
        start();
    }
}

void ShareWatch::start()
{
    if (m_asking) {
        return;
    }
    m_asking = true;

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
        m_asking = false;
        if (reply.isError()) {
            const QString message = reply.error().message().left(200);

            qCWarning(KDOS_KDED) << "Samba was not started for the new share:" << reply.error().name()
                                 << message;
            notifyFailure(message);
            return;
        }
        qCInfo(KDOS_KDED) << "a folder was shared; Samba is serving it";
    });
}

void ShareWatch::notifyFailure(const QString &message)
{
    /* Once per reason in a session: every later share would fail the same
     * way, for the same thing to fix. */
    if (m_reported.contains(message)) {
        return;
    }
    m_reported.insert(message);

    auto *n = new KNotification(QStringLiteral("shareStartFailed"));

    n->setComponentName(QStringLiteral("kded_kdos"));
    n->setTitle(i18nc("@title", "Shared folder not served"));
    n->setText(i18nc("@info %1 is the reason",
                     "The folder is shared, but the file server could not be started: %1",
                     message.toHtmlEscaped()));
    n->setIconName(QStringLiteral("folder-remote"));
    n->sendEvent();
}

}
