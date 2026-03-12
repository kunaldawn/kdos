#!/bin/bash
. /etc/init.d/service_helper

NAME="dmesg"

case "$1" in
    start)
        echo "[KDOS] Suppressing kernel messages on console..."
        dmesg -n 1
        ;;
    stop)
        # Restore default log level
        dmesg -n 7
        ;;
    status)
        echo "[ OK ] $NAME: console log level configured"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
