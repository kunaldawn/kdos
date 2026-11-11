/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — plasmashell brought back when it stops
 * ---------------------------------
 */

#include "shellwatch.h"
#include "debug.h"
#include "logic.h"

#include <KLocalizedString>
#include <KNotification>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QProcess>

namespace Kdos
{

static const QString SHELL = QStringLiteral("org.kde.plasmashell");

/* How long the name may be gone before the shell is started again:
 * `plasmashell --replace` and KCrash's own restart both leave a gap, and a
 * shell that takes the name back inside it is not started twice. A second
 * start inside the gap is harmless as well, because plasmashell is a unique
 * KDBusService and the later one exits. */
static constexpr int GRACE_MS = 3000;

/* At most this many starts in this many milliseconds: a shell that cannot
 * start would otherwise be restarted for ever. */
static constexpr int MAX_STARTS = 5;
static constexpr qint64 WINDOW_MS = 60 * 1000;

/* ksmserver answers at once; one that does not is a session going away. */
static constexpr int ASK_TIMEOUT_MS = 5000;

/* A shell that exits before it takes the name drops nothing the watcher
 * sees, so each start is followed by one look of its own. */
static constexpr int RECHECK_MS = 20 * 1000;

ShellWatch::ShellWatch(QObject *parent)
    : QObject(parent)
    , m_watcher(SHELL, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForUnregistration)
{
    m_grace.setSingleShot(true);
    m_grace.setInterval(GRACE_MS);
    connect(&m_grace, &QTimer::timeout, this, &ShellWatch::check);
    connect(&m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        m_grace.start();
    });
}

void ShellWatch::check()
{
    if (m_asking) {
        return;
    }
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();

    if (bus && bus->isServiceRegistered(SHELL)) {
        return;
    }
    m_asking = true;

    /* A logout closes plasmashell on purpose. ksmserver says so while it
     * runs one, and is gone, or does not answer, once the session ends. */
    const QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.ksmserver"),
                                                             QStringLiteral("/KSMServer"),
                                                             QStringLiteral("org.kde.KSMServerInterface"),
                                                             QStringLiteral("isShuttingDown"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, ASK_TIMEOUT_MS),
                                                this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<bool> reply = *w;

        w->deleteLater();
        m_asking = false;
        if (reply.isError()) {
            qCInfo(KDOS_KDED) << "plasmashell is gone and ksmserver did not answer"
                              << reply.error().name() << "- not starting it";
            return;
        }
        if (reply.value()) {
            return;
        }
        respawn();
    });
}

void ShellWatch::respawn()
{
    if (!startAllowed(m_starts, QDateTime::currentMSecsSinceEpoch(), MAX_STARTS, WINDOW_MS)) {
        qCWarning(KDOS_KDED) << "plasmashell stopped" << MAX_STARTS << "times in a minute; not starting it again";
        notifyLimit();
        return;
    }
    if (!QProcess::startDetached(QStringLiteral("plasmashell"), {})) {
        qCWarning(KDOS_KDED) << "plasmashell could not be started";
        return;
    }
    qCInfo(KDOS_KDED) << "plasmashell had stopped; started it again";
    QTimer::singleShot(RECHECK_MS, this, &ShellWatch::check);
}

void ShellWatch::notifyLimit()
{
    /* Once in a session: the next stop is for the same thing to fix. */
    if (m_reported) {
        return;
    }
    m_reported = true;

    auto *n = new KNotification(QStringLiteral("shellRespawnLimit"));

    n->setComponentName(QStringLiteral("kded_kdos"));
    n->setTitle(i18nc("@title", "Desktop shell keeps stopping"));
    n->setText(i18nc("@info",
                     "The panel and desktop stopped %1 times in a minute and were not started again. "
                     "Run kstart plasmashell, or log out and in.",
                     MAX_STARTS));
    n->setIconName(QStringLiteral("plasmashell"));
    n->sendEvent();
}

}
