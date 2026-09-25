#!/bin/bash
. /etc/init.d/service_helper

NAME="ipp-usb"
DAEMON="/usr/sbin/ipp-usb"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # standalone = run for ever and serve every IPP-over-USB device as it
        # is plugged in, in the foreground for supervision (no -bg). udev mode
        # exits with the last device, and nothing here starts it again.
        supervise "$NAME" "$DAEMON" standalone
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
