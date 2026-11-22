#!/bin/bash
. /etc/init.d/service_helper

NAME="mdns"
DAEMON="/usr/sbin/kdos-mdnsd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # Every answer is avahi-daemon's, so without it the daemon could
        # only say SERVFAIL. 45_avahi.sh sorts before this script.
        if [ ! -x /usr/sbin/avahi-daemon ]; then
            echo "[SKIP] $NAME: avahi-daemon not installed"
            exit 0
        fi
        if [ -e /etc/service.disabled/avahi ]; then
            echo "[SKIP] $NAME: avahi is disabled (/etc/service.disabled/avahi)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # Binds 127.0.0.1:5380 as root, then runs as `nobody`. dnsmasq
        # reaches it through /etc/NetworkManager/dnsmasq.d/kdos-mdns.conf.
        # --restart-with avahi-daemon: restarted with Avahi, and held while
        # Avahi is between two runs.
        supervise --restart-with avahi-daemon "$NAME" "$DAEMON"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
