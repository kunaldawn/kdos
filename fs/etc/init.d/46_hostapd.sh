#!/bin/bash
. /etc/init.d/service_helper

# A WIRELESS ACCESS POINT, AND ONLY WHEN SOMEBODY WROTE ONE. The package ships
# hostapd.conf.example and no hostapd.conf, so a machine nobody configured
# skips here rather than supervising a daemon that exits on a missing file.
#
# NetworkManager must be told to leave the interface alone —
# `unmanaged-devices=interface-name:<if>` in /etc/NetworkManager/conf.d —
# or it and hostapd both drive the same card. Addresses and names for the
# clients are a dnsmasq of the administrator's own, which needs
# `bind-interfaces` beside NetworkManager's loopback one, and the firewall does not
# let a hand-made subnet in: see docs/kdos/02-user-guide/administration.md.
# kdos-net's hotspot is the other way to an access point and needs none of it.

NAME="hostapd"
DAEMON="/usr/sbin/hostapd"
CONF="/etc/hostapd/hostapd.conf"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        if [ ! -f "$CONF" ]; then
            echo "[SKIP] $NAME: no $CONF"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # Foreground for supervision (no -B); -s logs to syslog, as there is
        # no terminal to log to.
        supervise "$NAME" "$DAEMON" -s "$CONF"
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
