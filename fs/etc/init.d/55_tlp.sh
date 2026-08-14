#!/bin/bash
. /etc/init.d/service_helper

NAME="tlp"
DAEMON="/usr/sbin/tlp"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # NOT supervised: tlp is not a daemon. It applies a set of /sys
        # settings and exits, so handing it to ksvc would make the supervisor
        # respawn it forever.
        echo "[KDOS] Applying $NAME power policy..."
        "$DAEMON" start
        ;;
    stop)
        # `tlp start` is also how you re-apply after a policy change; there is
        # no daemon to stop, so this restores the AC profile and exits.
        [ -x "$DAEMON" ] && "$DAEMON" start
        ;;
    status)
        [ -x /usr/bin/tlp-stat ] && tlp-stat -s || echo "$NAME: not installed"
        ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
