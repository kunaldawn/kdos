#!/bin/bash
. /etc/init.d/service_helper

NAME="dhcpcd"
DAEMON="/usr/sbin/dhcpcd"
NM_DAEMON="/usr/sbin/NetworkManager"
NM_DISABLED="/etc/service.disabled/networkmanager"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # One DHCP client per link. NetworkManager's DHCP client is internal
        # and never defers to a running dhcpcd, so both lease every interface:
        # two default routes installed and withdrawn on each renewal, and two
        # writers of /etc/resolv.conf. dhcpcd is the fallback for a machine
        # that does not run NetworkManager.
        #
        # The marker decides, not the binary: deselecting the service leaves
        # /usr/sbin/NetworkManager in place, and standing down on the binary
        # alone would leave such a machine with no DHCP client at all. The
        # name is the one rcS derives from 42_networkmanager.sh.
        if [ -x "$NM_DAEMON" ] && [ ! -f "$NM_DISABLED" ]; then
            echo "[SKIP] $NAME: NetworkManager owns the interfaces"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/dhcpcd
        # -B keeps dhcpcd in the foreground so ksvc supervises the daemon
        # itself rather than a parent that has already exited.
        supervise "$NAME" "$DAEMON" -B
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
