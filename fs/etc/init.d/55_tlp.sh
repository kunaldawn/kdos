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
        #
        # `init start`, not `start`: the init verb is the boot one, and is the
        # only one that applies DEVICES_TO_{DIS,EN}ABLE_ON_STARTUP and
        # RESTORE_DEVICE_STATE_ON_STARTUP. USB autosuspend is not applied here
        # — TLP's udev rule sets it per device as coldplug adds them.
        echo "[KDOS] Applying $NAME power policy..."
        "$DAEMON" init start
        ;;
    stop)
        # `init stop` is the shutdown half: it records the radio states the
        # next boot restores, applies DEVICES_TO_*_ON_SHUTDOWN and clears the
        # saved profile. `tlp start` in its place would re-apply a profile to
        # a machine that is about to lose power and leave the others unread.
        [ -x "$DAEMON" ] && "$DAEMON" init stop
        ;;
    status)
        [ -x /usr/bin/tlp-stat ] && tlp-stat -s || echo "$NAME: not installed"
        ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
