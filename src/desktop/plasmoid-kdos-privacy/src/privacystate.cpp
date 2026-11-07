/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   privacystate — what `kdos privacy --json` says is capturing
 */

#include "privacystate.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace Kdos
{

namespace
{
/* The output's version this reader understands. A newer helper that changes
 * a field's meaning raises it, and its lamps stay dark rather than wrong. */
constexpr int KnownVersion = 1;

/* Nothing an id or a name needs is longer; a longer one is not a helper's. */
constexpr qsizetype MaxField = 512;

QString field(const QJsonObject &o, const char *key)
{
    const QString s = o.value(QLatin1String(key)).toString();
    return s.size() > MaxField ? s.left(MaxField) : s;
}

void collect(const QJsonObject &root, CaptureKind kind, QList<Capture> &out)
{
    const QJsonArray list = root.value(kdosKindKey(kind)).toArray();
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        Capture c;
        c.kind = kind;
        c.id = field(o, "id");
        /* The id goes back to the helper as an argument of its own; one that
         * starts with `-` would be read as an option. */
        if (c.id.isEmpty() || c.id.startsWith(QLatin1Char('-'))) {
            continue;
        }
        c.app = field(o, "app");
        c.icon = field(o, "icon");
        c.box = field(o, "box");
        c.pid = o.value(QLatin1String("pid")).toInteger();
        c.muted = o.value(QLatin1String("muted")).toBool();
        c.canMute = kind == CaptureKind::Microphone && o.value(QLatin1String("mutable")).toBool();
        c.canStop = o.value(QLatin1String("stoppable")).toBool();
        out.append(c);
    }
}
} // namespace

bool PrivacyState::active(CaptureKind kind) const
{
    return std::any_of(captures.cbegin(), captures.cend(), [kind](const Capture &c) {
        return c.kind == kind;
    });
}

bool PrivacyState::allMuted(CaptureKind kind) const
{
    bool any = false;
    for (const Capture &c : captures) {
        if (c.kind != kind) {
            continue;
        }
        any = true;
        if (!c.muted) {
            return false;
        }
    }
    return any;
}

QList<Capture> PrivacyState::of(CaptureKind kind) const
{
    QList<Capture> out;
    std::copy_if(captures.cbegin(), captures.cend(), std::back_inserter(out), [kind](const Capture &c) {
        return c.kind == kind;
    });
    return out;
}

PrivacyState kdosParsePrivacy(const QByteArray &line)
{
    PrivacyState s;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(line.trimmed(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return s;
    }
    const QJsonObject root = doc.object();
    if (root.value(QLatin1String("version")).toInt() != KnownVersion) {
        return s;
    }
    s.valid = true;
    collect(root, CaptureKind::Microphone, s.captures);
    collect(root, CaptureKind::Camera, s.captures);
    collect(root, CaptureKind::Screen, s.captures);
    return s;
}

QString kdosCaptureLabel(const Capture &c)
{
    const QString app = c.app.isEmpty() ? i18ndc("kdos-plasmoid-privacy", "@info a capturing program with no name", "An unnamed program") : c.app;
    if (c.box.isEmpty()) {
        return app;
    }
    return i18ndc("kdos-plasmoid-privacy", "@info %1 application, %2 box", "%1 in box %2", app, c.box);
}

QString kdosKindKey(CaptureKind kind)
{
    switch (kind) {
    case CaptureKind::Microphone:
        return QStringLiteral("mic");
    case CaptureKind::Camera:
        return QStringLiteral("camera");
    case CaptureKind::Screen:
        return QStringLiteral("screen");
    }
    return {};
}

QStringList kdosMuteArguments(const QString &id, bool mute)
{
    return {QStringLiteral("privacy"), mute ? QStringLiteral("mute") : QStringLiteral("unmute"), id};
}

QStringList kdosStopArguments(const QString &id)
{
    return {QStringLiteral("privacy"), QStringLiteral("stop"), id};
}

QVariantMap kdosCaptureMap(const Capture &c)
{
    return {
        {QStringLiteral("kind"), kdosKindKey(c.kind)},
        {QStringLiteral("id"), c.id},
        {QStringLiteral("label"), kdosCaptureLabel(c)},
        {QStringLiteral("icon"), c.icon},
        {QStringLiteral("box"), c.box},
        {QStringLiteral("muted"), c.muted},
        {QStringLiteral("canMute"), c.canMute},
        {QStringLiteral("canStop"), c.canStop},
    };
}

} // namespace Kdos
