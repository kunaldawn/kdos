/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   privacyapplet — the privacy lamps' applet, what QML reads as Plasmoid
 */

#pragma once

#include <Plasma/Applet>

#include <QVariantList>

#include "privacywatch.h"

namespace Kdos
{

class PrivacyApplet : public Plasma::Applet
{
    Q_OBJECT
    /* Every capture, as kdosCaptureMap() describes one. */
    Q_PROPERTY(QVariantList captures READ captures NOTIFY stateChanged)
    Q_PROPERTY(bool microphoneActive READ microphoneActive NOTIFY stateChanged)
    Q_PROPERTY(bool microphoneMuted READ microphoneMuted NOTIFY stateChanged)
    Q_PROPERTY(bool cameraActive READ cameraActive NOTIFY stateChanged)
    Q_PROPERTY(bool screenActive READ screenActive NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY stateChanged)
    /* summary() of each kind, keyed mic, camera, screen: a property, so a
     * binding that reads it follows every change of state. */
    Q_PROPERTY(QVariantMap summaries READ summaries NOTIFY stateChanged)

public:
    PrivacyApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);

    void init() override;

    QVariantList captures() const;
    bool microphoneActive() const;
    bool microphoneMuted() const;
    bool cameraActive() const;
    bool screenActive() const;
    bool available() const;
    QString problem() const;
    QVariantMap summaries() const;

    /* "Microphone: Firefox in box www, Konsole". Empty when the kind is idle. */
    QString summary(const QString &kind) const;
    /* What a click on a lamp does: the microphone's captures are all muted,
     * or all unmuted when every one already is; each camera or screen capture
     * that can be stopped is stopped. */
    Q_INVOKABLE void actOn(const QString &kind);
    /* Only a capture the helper listed, and only what it says can be done to
     * it: an id from anywhere else is ignored. */
    Q_INVOKABLE void setMuted(const QString &id, bool muted);
    Q_INVOKABLE void stop(const QString &id);

Q_SIGNALS:
    void stateChanged();
    /* kdos could not be started for an action; the message says so. */
    void actionFailed(const QString &message);

private:
    void runKdos(const QStringList &arguments);

    PrivacyWatch m_watch;
};

} // namespace Kdos
