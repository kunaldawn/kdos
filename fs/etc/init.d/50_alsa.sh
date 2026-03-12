#!/bin/bash
. /etc/init.d/service_helper

NAME="alsa"

case "$1" in
    start)
        if ! command -v alsactl >/dev/null 2>&1; then
            echo "[SKIP] $NAME: alsactl not found"
            exit 0
        fi
        echo "[KDOS] Restoring ALSA mixer settings..."
        alsactl restore 2>/dev/null || true
        echo "[KDOS] $NAME ready"
        ;;
    stop)
        echo "[KDOS] Saving ALSA mixer settings..."
        alsactl store 2>/dev/null || true
        ;;
    status)
        if command -v alsactl >/dev/null 2>&1; then
            echo "[ OK ] $NAME: alsactl available"
        else
            echo "[DOWN] $NAME: alsactl not found"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
