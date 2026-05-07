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
        # Group is created on first install if missing.
        getent group seat >/dev/null 2>&1 || groupadd -r seat
        supervise "$NAME" "$DAEMON -g seat"
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
