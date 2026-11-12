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
 * Which boundary actually held for a running box.
 *
 * kdos-appbox records it for every launch in
 * $XDG_RUNTIME_DIR/kdos/boxes/<box>.state, as `key = value` lines: the
 * verdict (`confinement = confined | tagged | unconfined`), the sentence that
 * says why it is not confined (`reason =`), the launcher and the time. The
 * three words mean:
 *
 *   confined    its Wayland socket is tagged, its private runtime folder has
 *               neither the session's Wayland socket nor `bus`, its bus
 *               goes through xdg-dbus-proxy, and no X11 display is within its reach
 *   tagged      KWin's denylist applies to the tagged socket, but the box can
 *               reach past it: through a runtime folder shared with the
 *               session, an unfiltered bus or an X11 display
 *   unconfined  its last launch had no tagged socket, so nothing marks its
 *               windows as a box's
 *
 * NO FILE IS NO CLAIM. A box that is not running, or a runtime that recorded
 * nothing, reads as Unknown and the page says it cannot tell; it never reads
 * as confined. "Confined" in this page then means what it says.
 */

#pragma once

#include <QDateTime>
#include <QString>

namespace Kdos
{

enum class Confinement {
    Unknown,
    Confined,
    Tagged,
    Unconfined,
};

/* One state file, read. A key the file lacks is empty or invalid. */
struct ConfinementRecord {
    Confinement state = Confinement::Unknown;
    QString reason;
    QString launcher;
    QDateTime time;
};

/* $XDG_RUNTIME_DIR, else /run/user/<uid>: the directory kdos-appbox uses. */
QString kdosRuntimeDir();

/* The state file's path for a box. */
QString confinementStatePath(const QString &box);

/* Parses the state file's text. The verdict is the `confinement =` key, a
 * `state =` key, or a first line that is the word alone. */
ConfinementRecord parseConfinementRecord(const QString &text);
Confinement parseConfinement(const QString &text);

ConfinementRecord readConfinementRecord(const QString &box);

/* "confined", "tagged", "unconfined" or "". */
QString confinementId(Confinement c);

}
