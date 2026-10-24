#!/bin/bash
. /etc/init.d/service_helper

# The only thing that starts NetworkManager. The image ships no D-Bus
# activation file for org.freedesktop.NetworkManager: with one, the call
# kdos-netagent makes whenever the name has no owner would make the bus start
# a second NetworkManager outside the supervisor — at a login on a machine
# that disabled this service, and in the gap of a `service restart`, after
# which the supervised one exits "already running" every five seconds. A call
# made while the name is unowned is refused instead, and the caller retries.
#
# --restart-with dbus: NetworkManager does not exit when its system-bus
# connection closes. Without it, a restarted bus leaves NetworkManager running
# with no name on the new bus, wpa_supplicant gone with the old one, Wi-Fi
# down, and `service status` still [ OK ]. ModemManager, polkitd, avahi and
# bluetoothd exit with the bus and are restarted by their own supervisors.

NAME="NetworkManager"
DAEMON="/usr/sbin/NetworkManager"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise --restart-with dbus "$NAME" "$DAEMON" --no-daemon
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
