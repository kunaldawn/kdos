#!/bin/bash
. /etc/init.d/service_helper

NAME="bluetoothd"
DAEMON="/usr/lib/bluetooth/bluetoothd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            # Try sbin symlink
            DAEMON="/usr/sbin/bluetoothd"
        fi
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: daemon not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # -n = don't daemonise (stay foreground for supervision)
        supervise "$NAME" "$DAEMON -n"
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
