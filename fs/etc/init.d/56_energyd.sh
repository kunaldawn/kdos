#!/bin/bash
. /etc/init.d/service_helper

NAME="kdos-energyd"
DAEMON="/usr/sbin/kdos-energyd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # RAPL is what this daemon measures, and plenty of machines do not have
        # it — most VMs, and every non-x86 part. The daemon refuses to start
        # there rather than reporting a machine that uses no energy at all, and
        # `supervise` would respawn a refusing daemon forever. So the check is
        # here, before the respawn loop exists.
        FOUND=0
        for D in /sys/class/powercap/*/energy_uj; do
            [ -r "$D" ] && FOUND=1
        done
        if [ "$FOUND" = "0" ]; then
            echo "[SKIP] $NAME: no readable RAPL energy domain"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
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
