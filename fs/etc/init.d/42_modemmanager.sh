#!/bin/bash
. /etc/init.d/service_helper

# STARTED HERE RATHER THAN LEFT TO D-BUS ACTIVATION, for the reason polkitd is:
# `org.freedesktop.ModemManager1.service` is a `User=root` service, and
# activating one hangs off the setuid launch helper. A modem that is present
# and never managed says nothing anywhere.
#
# AFTER polkitd (41) AND BEFORE NetworkManager (42_networkmanager, which sorts
# after this name). NetworkManager's WWAN and Bluetooth DUN devices are
# ModemManager's modems, and a NetworkManager that found no ModemManager on the
# bus would poke the name — which is activation again. fwupd's modem plugin
# reads the same daemon.

NAME="ModemManager"
DAEMON="/usr/sbin/ModemManager"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # Foreground by default; it logs to syslog.
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
