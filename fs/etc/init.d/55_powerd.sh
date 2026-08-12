#!/bin/bash
. /etc/init.d/service_helper

NAME="kdos-powerd"
DAEMON="/usr/sbin/kdos-powerd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # Suspend and poweroff are root's, and the desktop is not root. The
        # daemon is the whole of what stands between them: a socket in /run
        # gated by SO_PEERCRED, root and wheel only. No setuid binary, no
        # polkit prompt (a lid-close cannot answer one), no logind.
        #
        # It runs in the foreground by design — `supervise` owns the respawn.
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
