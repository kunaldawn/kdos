#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   What a Plasma session does as it ends.
#
# plasma-shutdown runs every file in every plasma-workspace/shutdown directory
# of the XDG config directories at Log Out, Restart and Shut Down, after KWin
# has closed every window and before it quits. It runs each one directly, so
# it needs its execute bit, and waits for it, so it is bounded: a logout that
# hangs on a box is a logout that hangs.
#
# THE BOXES THE SESSION LEFT IDLE ARE GIVEN BACK. `kdos-box gc --session-end`
# stops every box that has run past its profile's `autostop`. `--session-end`
# is what tells it not to ask the desktop which boxes still have a window:
# every window is closed by now, and a desktop that is quitting may not
# answer, which a plain gc takes as "cannot tell" and stops nothing. A box
# with no `autostop` keeps running past this logout; every container of the
# user's stops at the user's LAST logout, with the user's services
# (/etc/kdos/user-services/40_boxes.sh), unless the user lingers.
#
# Its output reaches neither the session nor its log. plasma-shutdown is
# started by the session bus, which is the user service `dbus`, so what this
# prints is in that service's log, $XDG_RUNTIME_DIR/ksvc/kdos-svc.dbus.log,
# and in syslog under `dbus`.
# ---------------------------------

command -v kdos-box >/dev/null 2>&1 || exit 0
echo "kdos: session end: kdos-box gc --session-end" >&2
timeout 20 nice -n 10 kdos-box gc --session-end >&2
exit 0
