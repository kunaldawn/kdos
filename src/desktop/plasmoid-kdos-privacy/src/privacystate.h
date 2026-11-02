/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   privacystate — what `kdos privacy --json` says is capturing
 *
 * One line of the helper's output in, the captures out, and the argument
 * vectors that mute or stop one, as plain functions so the QtTest suite feeds
 * them lines and no test needs PipeWire, a camera or a session.
 *
 *   {"version":1,
 *    "mic":[{"id":"pw:57","app":"Firefox","icon":"firefox","box":"www",
 *            "pid":4242,"muted":false,"mutable":true,"stoppable":false}],
 *    "camera":[…], "screen":[…]}
 */

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Kdos
{

enum class CaptureKind {
    Microphone,
    Camera,
    Screen,
};

struct Capture {
    CaptureKind kind = CaptureKind::Microphone;
    QString id;
    QString app;
    QString icon;
    QString box;
    qint64 pid = 0;
    bool muted = false;
    bool canMute = false;
    bool canStop = false;
};

struct PrivacyState {
    bool valid = false;
    QList<Capture> captures;

    bool active(CaptureKind kind) const;
    /* Every microphone capture is muted; false while none is capturing. */
    bool allMuted(CaptureKind kind) const;
    QList<Capture> of(CaptureKind kind) const;
};

/* One line of `kdos privacy --json`. An entry with no id, or of a version this
 * reader does not know, is dropped; a line that is not an object is invalid. */
PrivacyState kdosParsePrivacy(const QByteArray &line);

/* "Firefox in box www", or the application alone when it is not boxed. */
QString kdosCaptureLabel(const Capture &c);

/* The kind's key in the helper's output and in the plasmoid: mic, camera,
 * screen. */
QString kdosKindKey(CaptureKind kind);

/* `kdos privacy mute|unmute|stop <id>`, as the arguments after `kdos`. */
QStringList kdosMuteArguments(const QString &id, bool mute);
QStringList kdosStopArguments(const QString &id);

/* A capture as QML reads it: kind, id, label, icon, box, muted, canMute,
 * canStop. */
QVariantMap kdosCaptureMap(const Capture &c);

} // namespace Kdos
