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
 * The client half of io.kdos.Slots1 (io.kdos.Slots1.xml beside this file).
 *
 * EVERY CALL IS ASYNCHRONOUS. A blocking call from the settings window's
 * thread freezes the window for as long as the server takes, and Try waits
 * on a polkit prompt that a person may leave open for minutes. Property reads
 * go through Properties.GetAll for the same reason: QDBusAbstractInterface's
 * property getters block.
 *
 * INTERACTIVE AUTHORISATION IS ASKED FOR on every method that changes
 * something. Without the flag polkit answers `auth_admin_keep` with a refusal
 * instead of a prompt, and the button looks broken.
 */

#pragma once

#include <QDBusConnection>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace Kdos
{

class SlotsClient : public QObject
{
    Q_OBJECT

public:
    enum class Request {
        Try,
        CancelTry,
        SetChannel,
    };
    Q_ENUM(Request)

    explicit SlotsClient(QObject *parent = nullptr);

    /* The system bus, or the session bus when KDOS_SLOTS1_BUS=session. */
    static QDBusConnection bus();
    static QString serviceName();
    static QString objectPath();
    static QString interfaceName();

    bool isAvailable() const;
    bool isLoaded() const;
    bool isConfigured() const;
    QString runningSlot() const;
    QString activeSlot() const;
    QString trySlot() const;
    uint tryAttempts() const;
    bool tryBootNext() const;
    QString channel() const;
    QVariantMap state() const;
    QList<QVariantMap> slotMaps() const;
    bool isBusy() const;

    /* Re-reads every property and the slot list. */
    void refresh();

    void requestTry(const QString &slot);
    void requestCancelTry();
    void requestSetChannel(const QString &channel);

Q_SIGNALS:
    void availableChanged();
    void loadedChanged();
    void propertiesUpdated();
    void slotsUpdated();
    void busyChanged();
    /* errorName is empty when the call succeeded. */
    void requestFinished(Kdos::SlotsClient::Request request, const QString &errorName, const QString &errorMessage);

private:
    void setAvailable(bool available);
    void applyProperties(const QVariantMap &props);
    void fetchProperties();
    void fetchSlots();
    void call(Request request, const QString &method, const QVariantList &args);

private Q_SLOTS:
    /* Connected by name through QDBusConnection::connect, which needs
     * slots with exactly the signal's D-Bus signature. */
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
    void onSlotsChanged();

private:
    QDBusServiceWatcher *m_watcher = nullptr;
    bool m_available = false;
    bool m_loaded = false;
    int m_inFlight = 0;
    QVariantMap m_props;
    QList<QVariantMap> m_slots;
};

}
