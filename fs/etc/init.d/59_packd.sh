#!/bin/bash
. /etc/init.d/service_helper

NAME="kdos-packd"
DAEMON="/usr/sbin/kdos-packd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # EROFS is what a pack IS. Without the module this daemon can list
        # packs and mount none of them, which is a daemon that cannot do its
        # job — and `supervise` would respawn a refusing one forever, so the
        # check belongs here, before the respawn loop exists. The same shape
        # 56_energyd.sh and 57_oomd.sh keep for their own reasons.
        if ! grep -qw erofs /proc/filesystems 2>/dev/null; then
            modprobe erofs 2>/dev/null
        fi
        if ! grep -qw erofs /proc/filesystems 2>/dev/null; then
            echo "[SKIP] $NAME: erofs unavailable"
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
