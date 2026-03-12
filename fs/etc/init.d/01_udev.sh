#!/bin/bash
. /etc/init.d/service_helper

NAME="udev"
DAEMON="/usr/sbin/udevd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -pv /run/udev
        chmod 755 /run/udev
        "$DAEMON" --daemon
        /usr/sbin/udevadm trigger
        /usr/sbin/udevadm settle
        echo "[KDOS] $NAME ready"
        ;;
    stop)
        echo "[KDOS] Stopping $NAME..."
        killall udevd 2>/dev/null
        ;;
    status)
        if pgrep -x udevd >/dev/null 2>&1; then
            echo "[ OK ] $NAME is running"
        else
            echo "[DOWN] $NAME"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
