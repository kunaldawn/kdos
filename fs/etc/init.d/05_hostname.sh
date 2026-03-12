#!/bin/bash
. /etc/init.d/service_helper

NAME="hostname"

case "$1" in
    start)
        echo "[KDOS] Setting hostname..."
        if [ -f /etc/hostname ]; then
            hostname -F /etc/hostname
            echo "[KDOS] Hostname set to: $(hostname)"
        else
            echo "[SKIP] $NAME: /etc/hostname not found"
        fi
        ;;
    stop)
        # One-shot — nothing to stop
        ;;
    status)
        echo "[ OK ] hostname: $(hostname)"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
