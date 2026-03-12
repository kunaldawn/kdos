#!/bin/bash
. /etc/init.d/service_helper

NAME="sysctl"

case "$1" in
    start)
        echo "[KDOS] Applying sysctl settings..."
        if [ -f /etc/sysctl.conf ]; then
            sysctl -p /etc/sysctl.conf
        else
            echo "[SKIP] $NAME: /etc/sysctl.conf not found"
        fi
        ;;
    stop)
        # One-shot — nothing to stop
        ;;
    status)
        echo "[ OK ] $NAME: applied"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
