#!/bin/bash
. /etc/init.d/service_helper

NAME="seatd"
DAEMON="/usr/bin/seatd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # -g seat: members of the 'seat' group can take seats without root.
        # The group ships in /etc/group (with the desktop user in it); this is
        # only a fallback for a tree that predates it. No getent on musl.
        grep -q '^seat:' /etc/group 2>/dev/null || groupadd -r seat
        supervise "$NAME" "$DAEMON" -g seat
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
