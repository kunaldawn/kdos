#!/bin/bash
. /etc/init.d/service_helper

# turnstiled: EVERY LOGIN'S RUNTIME DIRECTORY AND EVERY USER'S SERVICES.
#
# pam_turnstile in the login stacks (SDDM's, the greeter's, util-linux login's)
# connects to this daemon's socket, /run/turnstiled/control.sock. At a user's
# first login it makes /run/user/<uid>, runs the backend
# /usr/lib/turnstile/ksvc, which starts /etc/kdos/user-services under ksvc —
# the session bus, PipeWire, WirePlumber, pipewire-pulse, the user's timers —
# and holds the login until they answer; at the last logout it stops them and
# removes the directory, unless the user lingers. Configuration:
# /etc/turnstile/turnstiled.conf.
#
# THE GREETER CANNOT START WITHOUT IT. `session required pam_turnstile.so` is in
# /etc/pam.d/sddm-greeter because nothing else makes the `sddm` account's
# runtime directory, and the greeter's KWin refuses to run without one. Every
# login comes after rcS — SDDM and the gettys are inittab entries — so where
# this sits in the order matters for a lingering user only, whose services it
# starts as soon as it is up: late, after the system bus, elogind, the network
# and the pack daemon those services and their boxes talk to. And the socket is
# waited for before the boot goes on, so the first login never finds none.
#
# STOPPED EARLY, rcK running the table backwards: every user's services stop
# while the bus and elogind they talk to are still up. turnstiled's own
# SIGTERM ends each user's services through the backend's `stop`, so the stop
# grace is the backend's whole stop, every service's grace in it.

NAME="turnstiled"
DAEMON="/usr/bin/turnstiled"
SOCK="/run/turnstiled/control.sock"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise --stop-grace 60 "$NAME" "$DAEMON"
        for _ in $(seq 1 50); do
            [ -S "$SOCK" ] && exit 0
            sleep 0.1
        done
        echo "[WARN] $NAME: $SOCK did not appear within five seconds"
        exit 1
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
