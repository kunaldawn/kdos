#!/bin/bash
. /etc/init.d/service_helper

NAME="kdos-oomd"
DAEMON="/usr/sbin/kdos-oomd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # PSI is what this daemon blocks on, and a kernel without it (psi=0 on
        # the command line, or CONFIG_PSI off) cannot arm the trigger. The
        # daemon refuses to start there rather than protecting nothing, and
        # `supervise` would respawn a refusing daemon forever — so the check is
        # here, before the respawn loop exists.
        if [ ! -w /proc/pressure/memory ]; then
            echo "[SKIP] $NAME: no writable /proc/pressure/memory (PSI off)"
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
