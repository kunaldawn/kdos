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

#include "systemversions.h"

#include "slotinfo.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QProcess>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Kdos
{

namespace
{
const auto KindRelease = u"release"_s;
const auto KindFolder = u"folder"_s;
const auto KindAddress = u"address"_s;

/* What a person can act on, from what the server said. The server's own
 * sentence is kept where it is the diagnosis (kdos-bootctl's refusals); the
 * bus's plumbing names are not. */
QString describeError(SlotsClient::Request request, const QString &name, const QString &message)
{
    if (name == "org.freedesktop.DBus.Error.AccessDenied"_L1 || name == "org.freedesktop.PolicyKit1.Error.NotAuthorized"_L1
        || name == "org.freedesktop.DBus.Error.InteractiveAuthorizationRequired"_L1) {
        return request == SlotsClient::Request::SetChannel
            ? i18nc("@info", "Changing where updates come from needs an administrator's password, and none was given.")
            : i18nc("@info", "Changing the version this computer starts needs an administrator's password, and none was given.");
    }
    if (name == "org.freedesktop.DBus.Error.ServiceUnknown"_L1 || name == "org.freedesktop.DBus.Error.NameHasNoOwner"_L1
        || name == "org.freedesktop.DBus.Error.NoReply"_L1 || name == "org.freedesktop.DBus.Error.Disconnected"_L1) {
        return i18nc("@info", "The system versions service (kdos-sysupdated) is not answering, so nothing was changed.");
    }
    if (!message.isEmpty()) {
        return message;
    }
    return i18nc("@info %1 D-Bus error name", "The request was refused (%1).", name);
}

bool hasSpaceOrControl(const QString &s)
{
    for (const QChar c : s) {
        if (c.isSpace() || c.category() == QChar::Other_Control) {
            return true;
        }
    }
    return false;
}
}

SystemVersionsModule::SystemVersionsModule(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
    , m_client(new SlotsClient(this))
    , m_channelKind(KindRelease)
{
    setButtons(Help | Apply | Default);

    m_slots = {new SlotInfo(u"a"_s, this), new SlotInfo(u"b"_s, this)};

    connect(m_client, &SlotsClient::availableChanged, this, [this] {
        syncSlots();
        Q_EMIT availabilityChanged();
        Q_EMIT statusChanged();
    });
    connect(m_client, &SlotsClient::loadedChanged, this, &SystemVersionsModule::availabilityChanged);
    connect(m_client, &SlotsClient::propertiesUpdated, this, [this] {
        /* The first answer after a SetChannel is the server's own spelling
         * of what was written, and it replaces the edit. */
        if (m_awaitingChannel) {
            m_awaitingChannel = false;
            m_channelEdited = false;
        }
        syncChannelFromServer();
        Q_EMIT statusChanged();
    });
    connect(m_client, &SlotsClient::slotsUpdated, this, [this] {
        syncSlots();
        Q_EMIT statusChanged();
    });
    connect(m_client, &SlotsClient::busyChanged, this, &SystemVersionsModule::busyChanged);
    connect(m_client, &SlotsClient::requestFinished, this, &SystemVersionsModule::onRequestFinished);
}

SlotsClient *SystemVersionsModule::client() const
{
    return m_client;
}

bool SystemVersionsModule::isAvailable() const
{
    return m_client->isAvailable();
}

bool SystemVersionsModule::isLoaded() const
{
    return m_client->isLoaded();
}

bool SystemVersionsModule::isConfigured() const
{
    return m_client->isConfigured();
}

bool SystemVersionsModule::isBusy() const
{
    return m_client->isBusy();
}

QList<QObject *> SystemVersionsModule::slotList() const
{
    QList<QObject *> out;
    for (SlotInfo *s : m_slots) {
        out << s;
    }
    return out;
}

SlotInfo *SystemVersionsModule::slotById(const QString &id) const
{
    for (SlotInfo *s : m_slots) {
        if (s->slotId() == id) {
            return s;
        }
    }
    return nullptr;
}

void SystemVersionsModule::syncSlots()
{
    QMap<QString, QVariantMap> byId;
    for (const QVariantMap &m : m_client->slotMaps()) {
        byId.insert(m.value(u"id"_s).toString().toLower(), m);
    }
    for (SlotInfo *s : m_slots) {
        s->update(byId.value(s->slotId()));
    }
}

bool SystemVersionsModule::isTrialArmed() const
{
    const QVariantMap st = m_client->state();
    if (st.contains(u"rebootRequired"_s)) {
        return st.value(u"rebootRequired"_s).toBool();
    }
    const QString t = m_client->trySlot();
    return !t.isEmpty() && t != m_client->runningSlot();
}

bool SystemVersionsModule::isOnTrial() const
{
    const QVariantMap st = m_client->state();
    if (st.contains(u"onTrial"_s)) {
        return st.value(u"onTrial"_s).toBool();
    }
    const QString t = m_client->trySlot();
    return !t.isEmpty() && t == m_client->runningSlot();
}

bool SystemVersionsModule::isTrialConfirmedSoon() const
{
    return isOnTrial() && !m_client->state().value(u"blessGraphical"_s).toBool();
}

QString SystemVersionsModule::trialTitle() const
{
    const SlotInfo *s = slotById(m_client->trySlot());
    return s ? s->title() : QString();
}

QString SystemVersionsModule::fallbackTitle() const
{
    const SlotInfo *s = slotById(m_client->activeSlot());
    return s ? s->title() : QString();
}

QString SystemVersionsModule::rolledBackTitle() const
{
    const SlotInfo *s = slotById(m_client->state().value(u"rolledBack"_s).toString());
    return s ? s->title() : QString();
}

QString SystemVersionsModule::jobText() const
{
    const QVariantMap st = m_client->state();
    const QString job = st.value(u"job"_s).toString();
    const uint done = st.value(u"jobDone"_s).toUInt();
    const uint total = st.value(u"jobTotal"_s).toUInt();
    if (job == "acquire"_L1) {
        return total > 0 ? i18nc("@info %1 done, %2 total", "An update is being prepared in the other slot: %1 of %2", done, total)
                         : i18nc("@info", "An update is being prepared in the other slot");
    }
    if (job == "install"_L1) {
        return i18nc("@info", "An update is being made ready to start");
    }
    return QString();
}

int SystemVersionsModule::jobProgress() const
{
    return int(m_client->state().value(u"jobProgress"_s).toUInt());
}

QString SystemVersionsModule::offerVersion() const
{
    const QVariantMap st = m_client->state();
    if (!st.value(u"job"_s).toString().isEmpty()) {
        return QString();
    }
    return st.value(u"offer"_s).toString();
}

bool SystemVersionsModule::isUpdatePrepared() const
{
    return m_client->state().value(u"phase"_s).toString() == "acquired"_L1;
}

QString SystemVersionsModule::preparedVersion() const
{
    return isUpdatePrepared() ? m_client->state().value(u"version"_s).toString() : QString();
}

QString SystemVersionsModule::lastError() const
{
    const QVariantMap st = m_client->state();
    if (!st.value(u"job"_s).toString().isEmpty()) {
        return QString();
    }
    return st.value(u"error"_s).toString();
}

QString SystemVersionsModule::kindOfChannel(const QString &channel)
{
    if (channel.isEmpty()) {
        return KindRelease;
    }
    if (channel.startsWith(u'/')) {
        return KindFolder;
    }
    return KindAddress;
}

QString SystemVersionsModule::composeChannel(const QString &kind, const QString &location, QString *error)
{
    QString err;
    QString out;
    const QString loc = location.trimmed();

    if (kind == KindRelease) {
        out = QString();
    } else if (kind == KindFolder) {
        if (loc.isEmpty()) {
            err = i18nc("@info", "Choose the folder that holds the KDOS update files.");
        } else if (!loc.startsWith(u'/')) {
            err = i18nc("@info", "A folder is written as a full path, starting with /.");
        } else if (hasSpaceOrControl(loc)) {
            /* update.conf is one `key = value` line per setting and
             * kdos update reads the value as a URL, so a space or a line
             * break cannot be written into it. */
            err = i18nc("@info", "The folder's path contains a space or a control character, which an update location cannot hold.");
        }
        out = loc;
    } else if (kind == KindAddress) {
        const QUrl url(loc, QUrl::StrictMode);
        const QString scheme = url.scheme().toLower();
        if (loc.isEmpty()) {
            err = i18nc("@info", "Type the address the KDOS update files are published at.");
        } else if (hasSpaceOrControl(loc) || !url.isValid()) {
            err = i18nc("@info", "This is not an address an update can be read from.");
        } else if (scheme != "https"_L1 && scheme != "http"_L1 && scheme != "file"_L1) {
            err = i18nc("@info", "Updates are read from an https, http or file address.");
        } else if (scheme != "file"_L1 && url.host().isEmpty()) {
            err = i18nc("@info", "The address names no server.");
        }
        out = loc;
    } else {
        err = i18nc("@info", "Unknown update location.");
    }
    if (error) {
        *error = err;
    }
    return out;
}

QString SystemVersionsModule::channelKind() const
{
    return m_channelKind;
}

void SystemVersionsModule::setChannelKind(const QString &kind)
{
    if (kind == m_channelKind || (kind != KindRelease && kind != KindFolder && kind != KindAddress)) {
        return;
    }
    m_channelKind = kind;
    const QString saved = m_client->channel();
    /* A location typed for one kind means nothing for another: switching
     * back to the saved kind restores the saved location, any other switch
     * starts empty. */
    m_channelLocation = (kind != KindRelease && kindOfChannel(saved) == kind) ? saved : QString();
    m_channelEdited = true;
    Q_EMIT channelChanged();
    updateSaveState();
}

QString SystemVersionsModule::channelLocation() const
{
    return m_channelLocation;
}

void SystemVersionsModule::setChannelLocation(const QString &location)
{
    if (location == m_channelLocation) {
        return;
    }
    m_channelLocation = location;
    m_channelEdited = true;
    Q_EMIT channelChanged();
    updateSaveState();
}

QString SystemVersionsModule::channelError() const
{
    if (m_channelKind == KindRelease || m_channelLocation.trimmed().isEmpty()) {
        return QString();
    }
    QString err;
    composeChannel(m_channelKind, m_channelLocation, &err);
    return err;
}

QString SystemVersionsModule::savedChannelText() const
{
    const QString saved = m_client->channel();
    return saved.isEmpty() ? i18nc("@info update source", "KDOS releases") : saved;
}

void SystemVersionsModule::syncChannelFromServer()
{
    if (m_channelEdited) {
        updateSaveState();
        return;
    }
    const QString saved = m_client->channel();
    const QString kind = kindOfChannel(saved);
    const QString loc = kind == KindRelease ? QString() : saved;
    if (kind != m_channelKind || loc != m_channelLocation) {
        m_channelKind = kind;
        m_channelLocation = loc;
        Q_EMIT channelChanged();
    }
    updateSaveState();
}

void SystemVersionsModule::updateSaveState()
{
    setRepresentsDefaults(m_channelKind == KindRelease);
    if (m_awaitingChannel) {
        setNeedsSave(false);
        return;
    }
    const QString saved = m_client->channel();
    bool differs = m_channelKind != kindOfChannel(saved);
    if (!differs && m_channelKind != KindRelease) {
        differs = m_channelLocation.trimmed() != saved;
    }
    setNeedsSave(m_client->isAvailable() && differs);
}

QString SystemVersionsModule::message() const
{
    return m_message;
}

int SystemVersionsModule::messageType() const
{
    return m_messageType;
}

void SystemVersionsModule::setMessage(const QString &text, MessageType type)
{
    /* Announced even when it repeats: a message closed by hand is shown
     * again when the same thing happens again. */
    m_message = text;
    m_messageType = text.isEmpty() ? NoMessage : type;
    Q_EMIT messageChanged();
}

void SystemVersionsModule::dismissMessage()
{
    setMessage(QString(), NoMessage);
}

void SystemVersionsModule::load()
{
    m_channelEdited = false;
    m_awaitingChannel = false;
    dismissMessage();
    m_client->refresh();
    syncChannelFromServer();
}

void SystemVersionsModule::save()
{
    QString err;
    const QString channel = composeChannel(m_channelKind, m_channelLocation, &err);
    if (!err.isEmpty()) {
        setMessage(err, Error);
        setNeedsSave(true);
        return;
    }
    if (channel == m_client->channel()) {
        m_channelEdited = false;
        updateSaveState();
        return;
    }
    m_pendingChannel = channel;
    m_client->requestSetChannel(channel);
}

void SystemVersionsModule::defaults()
{
    setChannelKind(KindRelease);
}

void SystemVersionsModule::startNextTime(const QString &slotId)
{
    const SlotInfo *s = slotById(slotId);
    if (!s || !s->isBootable()) {
        setMessage(s && !s->reason().isEmpty() ? s->reason() : i18nc("@info", "That version cannot be started."), Error);
        return;
    }
    dismissMessage();
    m_client->requestTry(slotId);
}

void SystemVersionsModule::cancelTrial()
{
    dismissMessage();
    m_client->requestCancelTry();
}

void SystemVersionsModule::restartNow()
{
    /* The logout screen, not a reboot: it says which version the restart
     * will start, and it lets the person save their work first. */
    QDBusMessage msg = QDBusMessage::createMethodCall(u"org.kde.LogoutPrompt"_s, u"/LogoutPrompt"_s, u"org.kde.LogoutPrompt"_s, u"promptReboot"_s);
    auto *w = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            setMessage(i18nc("@info", "The restart screen did not open. Restart from the application launcher's Leave menu."), Warning);
        }
    });
}

void SystemVersionsModule::openUpdates()
{
    if (!QProcess::startDetached(u"plasma-discover"_s, {u"--mode"_s, u"update"_s})) {
        setMessage(i18nc("@info", "Discover could not be started, so updates cannot be checked from here."), Error);
    }
}

void SystemVersionsModule::onRequestFinished(SlotsClient::Request request, const QString &errorName, const QString &errorMessage)
{
    if (!errorName.isEmpty()) {
        setMessage(describeError(request, errorName, errorMessage), Error);
        if (request == SlotsClient::Request::SetChannel) {
            m_awaitingChannel = false;
            updateSaveState();
        }
        return;
    }
    switch (request) {
    case SlotsClient::Request::Try:
    case SlotsClient::Request::CancelTry:
        /* The trial banner reads the new state the client fetches next;
         * a second sentence here would repeat it. */
        break;
    case SlotsClient::Request::SetChannel: {
        m_awaitingChannel = true;
        updateSaveState();
        const QString shown = m_pendingChannel.isEmpty() ? i18nc("@info update source", "KDOS releases") : m_pendingChannel;
        setMessage(i18nc("@info %1 update location", "Updates now come from %1.", shown), Positive);
        break;
    }
    }
}

}
