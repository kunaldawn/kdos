#!/bin/bash
. /etc/init.d/service_helper

NAME="cupsd"
DAEMON="/usr/sbin/cupsd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/cups
        # -f = run in foreground for supervision
        supervise "$NAME" "$DAEMON -f"
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
