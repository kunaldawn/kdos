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
 *
 * System Versions (kcm_kdos_system): the two root slots, which one this
 * computer starts, a trial of the other, and where updates come from.
 *
 * TWO KINDS OF CHANGE, TWO PATHS. Where updates come from is a setting: it
 * is edited, and Apply writes it, which is the System Settings contract and
 * what Defaults and the unsaved-changes prompt rely on. Starting the other
 * version is an act, not a setting: it is armed by its own button behind its
 * own confirmation, takes effect at the next restart and is reported in the
 * banner, so Apply never reboots anybody's computer into a different system.
 *
 * NOTHING HERE TOUCHES THE BOOT STATE. Every change goes to kdos-sysupdated
 * over io.kdos.Slots1, which runs kdos-bootctl after polkit has agreed; the
 * module only asks and reports what the server answered.
 */

#pragma once

#include <KQuickConfigModule>

#include <QList>
#include <QString>

#include "slotsclient.h"

namespace Kdos
{

class SlotInfo;

class SystemVersionsModule : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY availabilityChanged)
    Q_PROPERTY(bool configured READ isConfigured NOTIFY statusChanged)
    Q_PROPERTY(bool live READ isLive NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QList<QObject *> slotList READ slotList CONSTANT)
    Q_PROPERTY(bool trialArmed READ isTrialArmed NOTIFY statusChanged)
    Q_PROPERTY(bool onTrial READ isOnTrial NOTIFY statusChanged)
    Q_PROPERTY(bool trialConfirmedSoon READ isTrialConfirmedSoon NOTIFY statusChanged)
    Q_PROPERTY(QString trialTitle READ trialTitle NOTIFY statusChanged)
    Q_PROPERTY(QString fallbackTitle READ fallbackTitle NOTIFY statusChanged)
    Q_PROPERTY(QString rolledBackTitle READ rolledBackTitle NOTIFY statusChanged)
    Q_PROPERTY(QString jobText READ jobText NOTIFY statusChanged)
    Q_PROPERTY(int jobProgress READ jobProgress NOTIFY statusChanged)
    Q_PROPERTY(QString offerVersion READ offerVersion NOTIFY statusChanged)
    Q_PROPERTY(bool updatePrepared READ isUpdatePrepared NOTIFY statusChanged)
    Q_PROPERTY(QString preparedVersion READ preparedVersion NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(QString channelKind READ channelKind WRITE setChannelKind NOTIFY channelChanged)
    Q_PROPERTY(QString channelLocation READ channelLocation WRITE setChannelLocation NOTIFY channelChanged)
    Q_PROPERTY(QString channelError READ channelError NOTIFY channelChanged)
    Q_PROPERTY(QString savedChannelText READ savedChannelText NOTIFY statusChanged)
    Q_PROPERTY(QString installRoute READ installRoute WRITE setInstallRoute NOTIFY routeChanged)
    Q_PROPERTY(QString buildDeps READ buildDeps WRITE setBuildDeps NOTIFY routeChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(int messageType READ messageType NOTIFY messageChanged)

public:
    /* Kirigami.MessageType's values, so QML passes the number straight
     * through. */
    enum MessageType {
        NoMessage = -1,
        Information = 0,
        Positive = 1,
        Warning = 2,
        Error = 3,
    };
    Q_ENUM(MessageType)

    SystemVersionsModule(QObject *parent, const KPluginMetaData &metaData);

    bool isAvailable() const;
    bool isLoaded() const;
    bool isConfigured() const;
    bool isLive() const;
    bool isBusy() const;
    QList<QObject *> slotList() const;
    bool isTrialArmed() const;
    bool isOnTrial() const;
    bool isTrialConfirmedSoon() const;
    QString trialTitle() const;
    QString fallbackTitle() const;
    QString rolledBackTitle() const;
    QString jobText() const;
    int jobProgress() const;
    /* What Discover's update would bring, while nothing is under way. */
    QString offerVersion() const;
    /* An update prepared in the other slot and waiting for Install, and its
     * version ("" when the server did not name one). */
    bool isUpdatePrepared() const;
    QString preparedVersion() const;
    /* Why the last update stopped, while no job runs. */
    QString lastError() const;

    QString channelKind() const;
    void setChannelKind(const QString &kind);
    QString channelLocation() const;
    void setChannelLocation(const QString &location);
    QString channelError() const;
    QString savedChannelText() const;

    /* How software is installed when nothing names a route, and how a
     * source install gets what it lacks: "prebuilt" or "source" each. */
    QString installRoute() const;
    void setInstallRoute(const QString &route);
    QString buildDeps() const;
    void setBuildDeps(const QString &route);

    QString message() const;
    int messageType() const;

    /* "" / an absolute path / a URL, from a kind and a location; an empty
     * error means the pair is writable. Public for the tests. */
    static QString composeChannel(const QString &kind, const QString &location, QString *error);
    static QString kindOfChannel(const QString &channel);

    Q_INVOKABLE void startNextTime(const QString &slotId);
    Q_INVOKABLE void cancelTrial();
    Q_INVOKABLE void restartNow();
    Q_INVOKABLE void openUpdates();
    Q_INVOKABLE void openInstaller();
    Q_INVOKABLE void dismissMessage();

    SlotsClient *client() const;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

Q_SIGNALS:
    void availabilityChanged();
    void statusChanged();
    void busyChanged();
    void channelChanged();
    void routeChanged();
    void messageChanged();

private:
    SlotInfo *slotById(const QString &id) const;
    void syncSlots();
    void syncChannelFromServer();
    void syncRouteFromServer();
    void updateSaveState();
    void setMessage(const QString &text, MessageType type);
    void onRequestFinished(SlotsClient::Request request, const QString &errorName, const QString &errorMessage);

    SlotsClient *m_client = nullptr;
    QList<SlotInfo *> m_slots;
    QString m_channelKind;
    QString m_channelLocation;
    bool m_channelEdited = false;
    bool m_awaitingChannel = false;
    QString m_pendingChannel;
    QString m_installRoute = QStringLiteral("prebuilt");
    QString m_buildDeps = QStringLiteral("prebuilt");
    bool m_routeEdited = false;
    bool m_awaitingRoute = false;
    QString m_message;
    MessageType m_messageType = NoMessage;
};

}
