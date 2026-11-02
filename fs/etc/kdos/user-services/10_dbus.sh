#!/bin/bash
. /etc/init.d/service_helper

# THE SESSION BUS, ONE PER USER, AT THE FIXED PATH $XDG_RUNTIME_DIR/bus.
#
# kdos-appbox hands every box DBUS_SESSION_BUS_ADDRESS=unix:path=
# /run/user/<uid>/bus and binds /run/user/<uid> into it, so this is the path
# the host's side has to be at: a bus anywhere else leaves alien apps pointed
# at a socket nobody answers — GApplication single-instance breaks, dconf and
# the accessibility probe stall, notifications go nowhere. turnstile exports
# this same address into the PAM environment of every login once the socket
# exists, which is why the backend waits for `ready` below before it lets the
# login go on: Plasma's plasma-dbus-run-session-if-needed starts a private bus
# only when the variable is empty, and a private bus is one no box can reach.
#
# The address carries no guid, so a bus restarted by the supervisor at the
# same path is one every client reconnects to; zbus refuses a reconnection
# whose guid changed ("Server GUID mismatch").
#
# The daemon's output, and that of every program the bus activates, goes to
# the supervisor's log: $XDG_RUNTIME_DIR/ksvc/kdos-svc.dbus.log and syslog.

NAME="dbus"
DAEMON="/usr/bin/dbus-daemon"
BUS="unix:path=$XDG_RUNTIME_DIR/bus"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        if [ -z "$XDG_RUNTIME_DIR" ] || [ ! -d "$XDG_RUNTIME_DIR" ]; then
            echo "[FAIL] $NAME: no runtime directory; turnstile makes it at the first login"
            exit 1
        fi
        echo "[KDOS] Starting $NAME..."
        # --nofork keeps it in the foreground for supervision. libdbus
        # removes a stale socket at the path before it binds, so the
        # supervisor's restart never meets "address in use"; `supervise`
        # starting nothing for a name it already watches is the guard
        # against a second bus taking the socket from the first.
        supervise "$NAME" "$DAEMON" --session --nofork --address="$BUS"
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    ready)
        # 0 once the bus answers, 75 while it does not yet; 0 as well when
        # there is no supervised bus to wait for.
        check_status "$NAME" >/dev/null 2>&1 || exit 0
        dbus-send --bus="$BUS" --print-reply --dest=org.freedesktop.DBus \
            / org.freedesktop.DBus.Peer.Ping >/dev/null 2>&1 || exit 75
        ;;
    *)
        echo "Usage: $0 {start|stop|status|ready}"
        exit 1
        ;;
esac
