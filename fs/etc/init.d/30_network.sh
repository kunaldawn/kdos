#!/bin/bash
. /etc/init.d/service_helper

NAME="dhcpcd"
DAEMON="/usr/sbin/dhcpcd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/dhcpcd
        # -B = foreground for supervision; -A = ARP check/release
        supervise "$NAME" "$DAEMON -B"
        ;;
    stop)
        stop_service "$NAME"
        # Also ask dhcpcd to release leases gracefully
        "$DAEMON" -k 2>/dev/null || true
        ;;
    status)
        check_status "$NAME"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
