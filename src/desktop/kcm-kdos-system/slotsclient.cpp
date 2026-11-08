/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

#include "slotsclient.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

using namespace Qt::StringLiterals;

namespace Kdos
{

namespace
{
/* A polkit prompt is answered by a person, so the reply may take as long as
 * they do; D-Bus's own 25 s default would time a typed password out. */
constexpr int RequestTimeoutMs = 5 * 60 * 1000;

const auto PropertiesInterface = u"org.freedesktop.DBus.Properties"_s;

/* Nested containers arrive as QDBusArgument inside a QVariant; a map whose
 * values are left in that form reads as empty in QML. */
QVariantMap demarshalMap(const QVariant &v)
{
    if (v.canConvert<QDBusArgument>()) {
        return qdbus_cast<QVariantMap>(v.value<QDBusArgument>());
    }
    return v.toMap();
}

bool isGone(const QString &errorName)
{
    return errorName == "org.freedesktop.DBus.Error.ServiceUnknown"_L1 || errorName == "org.freedesktop.DBus.Error.NameHasNoOwner"_L1
        || errorName == "org.freedesktop.DBus.Error.NoReply"_L1 || errorName == "org.freedesktop.DBus.Error.Disconnected"_L1;
}
}

SlotsClient::SlotsClient(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<QList<QVariantMap>>();

    QDBusConnection b = bus();
    m_watcher = new QDBusServiceWatcher(serviceName(), b, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        if (newOwner.isEmpty()) {
            setAvailable(false);
        } else {
            refresh();
        }
    });

    b.connect(serviceName(),
              objectPath(),
              PropertiesInterface,
              u"PropertiesChanged"_s,
              this,
              SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    b.connect(serviceName(), objectPath(), interfaceName(), u"SlotsChanged"_s, this, SLOT(onSlotsChanged()));
}

QDBusConnection SlotsClient::bus()
{
    if (qEnvironmentVariable("KDOS_SLOTS1_BUS") == "session"_L1) {
        return QDBusConnection::sessionBus();
    }
    return QDBusConnection::systemBus();
}

QString SlotsClient::serviceName()
{
    return u"io.kdos.Slots1"_s;
}

QString SlotsClient::objectPath()
{
    return u"/io/kdos/Slots1"_s;
}

QString SlotsClient::interfaceName()
{
    return u"io.kdos.Slots1"_s;
}

bool SlotsClient::isAvailable() const
{
    return m_available;
}

bool SlotsClient::isLoaded() const
{
    return m_loaded;
}

bool SlotsClient::isConfigured() const
{
    return m_available && m_props.value(u"Configured"_s).toBool();
}

QString SlotsClient::runningSlot() const
{
    return m_props.value(u"RunningSlot"_s).toString();
}

QString SlotsClient::activeSlot() const
{
    return m_props.value(u"ActiveSlot"_s).toString();
}

QString SlotsClient::trySlot() const
{
    return m_props.value(u"TrySlot"_s).toString();
}

uint SlotsClient::tryAttempts() const
{
    return m_props.value(u"TryAttempts"_s).toUInt();
}

bool SlotsClient::tryBootNext() const
{
    return m_props.value(u"TryBootNext"_s).toBool();
}

QString SlotsClient::channel() const
{
    return m_props.value(u"Channel"_s).toString();
}

QVariantMap SlotsClient::state() const
{
    return m_props.value(u"State"_s).toMap();
}

QList<QVariantMap> SlotsClient::slotMaps() const
{
    return m_slots;
}

bool SlotsClient::isBusy() const
{
    return m_inFlight > 0;
}

void SlotsClient::setAvailable(bool available)
{
    if (!available) {
        m_props.clear();
        m_slots.clear();
    }
    const bool changed = m_available != available;
    m_available = available;
    if (!m_loaded) {
        m_loaded = true;
        Q_EMIT loadedChanged();
    }
    if (changed) {
        Q_EMIT availableChanged();
    }
    if (!available) {
        Q_EMIT propertiesUpdated();
        Q_EMIT slotsUpdated();
    }
}

void SlotsClient::refresh()
{
    fetchProperties();
    fetchSlots();
}

void SlotsClient::applyProperties(const QVariantMap &props)
{
    for (auto it = props.cbegin(); it != props.cend(); ++it) {
        if (it.key() == "State"_L1) {
            m_props.insert(it.key(), demarshalMap(it.value()));
        } else {
            m_props.insert(it.key(), it.value());
        }
    }
}

void SlotsClient::fetchProperties()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(serviceName(), objectPath(), PropertiesInterface, u"GetAll"_s);
    msg.setArguments({interfaceName()});
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *w;
        if (reply.isError()) {
            setAvailable(false);
            return;
        }
        m_props.clear();
        applyProperties(reply.value());
        setAvailable(true);
        Q_EMIT propertiesUpdated();
    });
}

void SlotsClient::fetchSlots()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(serviceName(), objectPath(), interfaceName(), u"ListSlots"_s);
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QList<QVariantMap>> reply = *w;
        if (reply.isError()) {
            if (isGone(reply.error().name())) {
                setAvailable(false);
            }
            return;
        }
        m_slots = reply.value();
        Q_EMIT slotsUpdated();
    });
}

void SlotsClient::call(Request request, const QString &method, const QVariantList &args)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(serviceName(), objectPath(), interfaceName(), method);
    msg.setArguments(args);
    msg.setInteractiveAuthorizationAllowed(true);

    ++m_inFlight;
    if (m_inFlight == 1) {
        Q_EMIT busyChanged();
    }
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg, RequestTimeoutMs), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, request](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<> reply = *w;
        --m_inFlight;
        if (m_inFlight == 0) {
            Q_EMIT busyChanged();
        }
        if (reply.isError()) {
            if (isGone(reply.error().name())) {
                setAvailable(false);
            }
            Q_EMIT requestFinished(request, reply.error().name(), reply.error().message());
            return;
        }
        /* The server announces what changed, but a client that waited for
         * the announcement alone would show the old state whenever the
         * signal is filtered or lost: read it back as well. */
        refresh();
        Q_EMIT requestFinished(request, QString(), QString());
    });
}

void SlotsClient::requestTry(const QString &slot)
{
    call(Request::Try, u"Try"_s, {slot});
}

void SlotsClient::requestCancelTry()
{
    call(Request::CancelTry, u"CancelTry"_s, {});
}

void SlotsClient::requestSetChannel(const QString &channel)
{
    call(Request::SetChannel, u"SetChannel"_s, {channel});
}

void SlotsClient::onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    if (interface != interfaceName()) {
        return;
    }
    applyProperties(changed);
    if (!invalidated.isEmpty()) {
        fetchProperties();
    }
    Q_EMIT propertiesUpdated();
}

void SlotsClient::onSlotsChanged()
{
    fetchSlots();
}

}
