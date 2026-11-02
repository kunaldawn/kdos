/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   polkitcheck — org.freedesktop.PolicyKit1.Authority.CheckAuthorization
 *
 * CheckAuthorization((sa{sv}) subject, s action, a{ss} details, u flags,
 * s cancellation) -> (bba{ss}): authorised, challenge, details. Flags 0 is
 * "do not interact".
 */

#include "polkitcheck.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QMap>
#include <QVariantMap>

namespace Kdos
{

namespace
{
constexpr int CheckTimeoutMs = 5000;
using StringMap = QMap<QString, QString>;
}

PolkitCheck::PolkitCheck(const QDBusConnection &bus, const QString &actionId, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_actionId(actionId)
{
    qDBusRegisterMetaType<StringMap>();
}

void PolkitCheck::check()
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.PolicyKit1"),
                                                       QStringLiteral("/org/freedesktop/PolicyKit1/Authority"),
                                                       QStringLiteral("org.freedesktop.PolicyKit1.Authority"),
                                                       QStringLiteral("CheckAuthorization"));
    QDBusArgument subject;
    subject.beginStructure();
    subject << QStringLiteral("system-bus-name") << QVariantMap{{QStringLiteral("name"), m_bus.baseService()}};
    subject.endStructure();
    call << QVariant::fromValue(subject) << m_actionId << QVariant::fromValue(StringMap()) << quint32(0) << QString();

    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, CheckTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusMessage reply = w->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            Q_EMIT answered(Answer::Unreachable);
            return;
        }
        bool authorised = false;
        bool challenge = false;
        StringMap details;
        const QDBusArgument result = reply.arguments().constFirst().value<QDBusArgument>();
        result.beginStructure();
        result >> authorised >> challenge >> details;
        result.endStructure();
        Q_EMIT answered(authorised ? Answer::Yes : challenge ? Answer::Challenge : Answer::No);
    });
}

} // namespace Kdos
