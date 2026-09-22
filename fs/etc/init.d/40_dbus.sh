#!/bin/bash
. /etc/init.d/service_helper

NAME="dbus"
DAEMON="/usr/bin/dbus-daemon"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/dbus /var/lib/dbus
        # GetMachineId fails without this file and the bus never creates it
        # itself; the id must differ per machine, so it is made on first boot
        # and never carried in an image.
        [ ! -f /var/lib/dbus/machine-id ] && dbus-uuidgen --ensure
        # --nofork keeps it in foreground for supervision
        supervise "$NAME" "$DAEMON" --system --nofork
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
