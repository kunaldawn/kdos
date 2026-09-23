#!/bin/bash
. /etc/init.d/service_helper

NAME="cups-browsed"
DAEMON="/usr/sbin/cups-browsed"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # cups-browsed never forks, so it runs in the foreground for
        # supervision with no flag. After 80_cups: it adds the printers it
        # finds to that cupsd.
        supervise "$NAME" "$DAEMON"
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
