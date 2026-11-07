/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   privacyapplet — the privacy lamps' applet, what QML reads as Plasmoid
 */

#include "privacyapplet.h"

#include <KLocalizedString>
#include <KPluginFactory>

namespace Kdos
{

namespace
{
CaptureKind kindOf(const QString &key)
{
    if (key == QLatin1String("camera")) {
        return CaptureKind::Camera;
    }
    if (key == QLatin1String("screen")) {
        return CaptureKind::Screen;
    }
    return CaptureKind::Microphone;
}
} // namespace

PrivacyApplet::PrivacyApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
{
    connect(&m_watch, &PrivacyWatch::changed, this, &PrivacyApplet::stateChanged);
}

void PrivacyApplet::init()
{
    Plasma::Applet::init();
    m_watch.start();
}

QVariantList PrivacyApplet::captures() const
{
    QVariantList out;
    for (const Capture &c : m_watch.state().captures) {
        out.append(kdosCaptureMap(c));
    }
    return out;
}

bool PrivacyApplet::microphoneActive() const
{
    return m_watch.state().active(CaptureKind::Microphone);
}

bool PrivacyApplet::microphoneMuted() const
{
    return m_watch.state().allMuted(CaptureKind::Microphone);
}

bool PrivacyApplet::cameraActive() const
{
    return m_watch.state().active(CaptureKind::Camera);
}

bool PrivacyApplet::screenActive() const
{
    return m_watch.state().active(CaptureKind::Screen);
}

bool PrivacyApplet::available() const
{
    return m_watch.available();
}

QString PrivacyApplet::problem() const
{
    return m_watch.problem();
}

QVariantMap PrivacyApplet::summaries() const
{
    QVariantMap out;
    for (CaptureKind k : {CaptureKind::Microphone, CaptureKind::Camera, CaptureKind::Screen}) {
        out.insert(kdosKindKey(k), summary(kdosKindKey(k)));
    }
    return out;
}

QString PrivacyApplet::summary(const QString &kind) const
{
    const QList<Capture> list = m_watch.state().of(kindOf(kind));
    if (list.isEmpty()) {
        return {};
    }
    QStringList names;
    for (const Capture &c : list) {
        names.append(c.muted ? i18ndc("kdos-plasmoid-privacy", "@info %1 a capturing program, muted", "%1 (muted)", kdosCaptureLabel(c))
                             : kdosCaptureLabel(c));
    }
    const QString who = names.join(i18ndc("kdos-plasmoid-privacy", "@info separator between programs", ", "));
    switch (kindOf(kind)) {
    case CaptureKind::Camera:
        return i18ndc("kdos-plasmoid-privacy", "@info:tooltip %1 the programs", "Camera: %1", who);
    case CaptureKind::Screen:
        return i18ndc("kdos-plasmoid-privacy", "@info:tooltip %1 the programs", "Screen: %1", who);
    case CaptureKind::Microphone:
        break;
    }
    return i18ndc("kdos-plasmoid-privacy", "@info:tooltip %1 the programs", "Microphone: %1", who);
}

void PrivacyApplet::actOn(const QString &kind)
{
    const CaptureKind k = kindOf(kind);
    const QList<Capture> list = m_watch.state().of(k);
    if (k == CaptureKind::Microphone) {
        const bool mute = !m_watch.state().allMuted(k);
        for (const Capture &c : list) {
            if (c.canMute && c.muted != mute) {
                runKdos(kdosMuteArguments(c.id, mute));
            }
        }
        return;
    }
    for (const Capture &c : list) {
        if (c.canStop) {
            runKdos(kdosStopArguments(c.id));
        }
    }
}

void PrivacyApplet::setMuted(const QString &id, bool muted)
{
    for (const Capture &c : m_watch.state().captures) {
        if (c.id == id && c.canMute) {
            runKdos(kdosMuteArguments(id, muted));
            return;
        }
    }
}

void PrivacyApplet::stop(const QString &id)
{
    for (const Capture &c : m_watch.state().captures) {
        if (c.id == id && c.canStop) {
            runKdos(kdosStopArguments(id));
            return;
        }
    }
}

void PrivacyApplet::runKdos(const QStringList &arguments)
{
    if (!m_watch.run(arguments)) {
        Q_EMIT actionFailed(i18ndc("kdos-plasmoid-privacy", "@info", "Could not run kdos to change the capture."));
    }
}

} // namespace Kdos

using Kdos::PrivacyApplet;
K_PLUGIN_CLASS_WITH_JSON(PrivacyApplet, "../metadata.json")

#include "privacyapplet.moc"
