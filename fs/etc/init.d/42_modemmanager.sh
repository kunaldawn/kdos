#!/bin/bash
. /etc/init.d/service_helper

# STARTED HERE AND NEVER BY D-BUS ACTIVATION, for the reason polkitd is:
# activating a root service hangs off the setuid launch helper, and a modem
# that is present and never managed says nothing anywhere. The port ships no
# activation file. With one, NetworkManager's call to the name as it starts
# would make the bus run a second ModemManager outside the supervisor; the
# supervised one would lose the name, exit and be started again every five
# seconds, never getting it.
#
# AFTER polkitd (41) AND BEFORE NetworkManager (42_networkmanager, which sorts
# after this name), and `wait_bus_name` holds the boot until the name is
# owned. NetworkManager's WWAN and Bluetooth DUN devices are ModemManager's
# modems; a NetworkManager that found the name unowned would stop asking for
# it and wait for it to appear. fwupd's modem plugin reads the same daemon.

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
        wait_bus_name org.freedesktop.ModemManager1
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
