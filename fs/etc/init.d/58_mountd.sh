#!/bin/bash
. /etc/init.d/service_helper

NAME="kdos-mountd"
DAEMON="/usr/sbin/kdos-mountd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # Mounting is root's and the desktop is not root; there is no udisks
        # here and there is not going to be one. The daemon is the whole of
        # what stands between them: a socket in /run gated by SO_PEERCRED,
        # root and wheel only, and a protocol whose only argument is a row
        # number out of a list the daemon itself published — so there is no
        # path a client can aim anywhere.
        #
        # Foreground by design: `supervise` owns the respawn.
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
