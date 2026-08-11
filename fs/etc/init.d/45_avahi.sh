#!/bin/bash
. /etc/init.d/service_helper

NAME="avahi-daemon"
DAEMON="/usr/sbin/avahi-daemon"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # After 40_dbus: avahi is a system-bus service and exits immediately
        # if it cannot claim org.freedesktop.Avahi.
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/avahi-daemon
        chown avahi:avahi /run/avahi-daemon 2>/dev/null
        # -f = foreground, for ksvc. Not --daemonize.
        supervise "$NAME" "$DAEMON" -f
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
