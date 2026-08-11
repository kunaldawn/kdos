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
        # A live ISO has no /var/lib/alsa/asound.state, and a failed restore
        # leaves the card exactly as the kernel left it — which for HDA is
        # Master MUTED at 0%/-74dB. Every PCM then opens, reports RUNNING and
        # plays silence, so the card looks present and working while nothing
        # comes out. That was "no audio on the TTY"; the desktop was fine only
        # because pipewire drives the mixer itself. `alsactl init` applies
        # /usr/share/alsa/init/, and answers 99 when it matched a generic rule
        # rather than a card-specific one — that is a success here, so its
        # status is deliberately ignored.
        if ! alsactl restore >/dev/null 2>&1; then
            echo "[KDOS] No saved mixer state — applying card defaults..."
            alsactl init >/dev/null 2>&1 || true
        fi
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
