#!/bin/bash
. /etc/init.d/service_helper

NAME="pcscd"
DAEMON="/usr/sbin/pcscd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # After polkitd: every client is authorised through polkit, and a
        # daemon up before it refuses them all. --foreground for supervision;
        # readers come and go through udev hotplug, so it runs for ever.
        install -d -m 755 /run/pcscd
        supervise "$NAME" "$DAEMON" --foreground
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
