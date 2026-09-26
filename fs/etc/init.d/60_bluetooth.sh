#!/bin/bash
. /etc/init.d/service_helper

NAME="bluetoothd"
DAEMON="/usr/lib/bluetooth/bluetoothd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            # Try sbin symlink
            DAEMON="/usr/sbin/bluetoothd"
        fi
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: daemon not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # -n = don't daemonise (stay foreground for supervision)
        #
        # -E and the ISO-socket kernel feature are what LE Audio needs: bluez
        # registers the LC3 (BAP) endpoints PipeWire offers only when both are
        # on, and without them an LE Audio headset or hearing aid falls back
        # to classic A2DP/HFP or does not connect. -K takes its argument only
        # in the `=` form. Flags rather than a main.conf here, which would
        # shadow the one the bluez package owns.
        supervise "$NAME" "$DAEMON" -n -E \
            --kernel=6fbaf188-05e0-496a-9885-d6ddfdb4e03e
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
