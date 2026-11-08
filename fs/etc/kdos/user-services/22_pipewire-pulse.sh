#!/bin/bash
. /etc/init.d/service_helper

# THE PULSEAUDIO SERVER EVERY PULSE CLIENT FINDS, plasma-pa among them: the
# panel's volume applet, the Audio page in System Settings and the per-app
# streams all talk PulseAudio. Its `ready` is what the turnstile backend waits
# for before the login goes on, so $XDG_RUNTIME_DIR/pulse/native answers
# before startplasma-wayland runs and plasma-pa never starts against nothing.
# Boxed applications find the same socket through the runtime directory every
# box binds.
#
# `--restart-with pipewire`: it is a client of the daemon and does not exit
# when the daemon goes away.

NAME="pipewire-pulse"
DAEMON="/usr/bin/pipewire-pulse"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise --restart-with pipewire "$NAME" "$DAEMON"
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    ready)
        check_status "$NAME" >/dev/null 2>&1 || exit 0
        [ -S "$XDG_RUNTIME_DIR/pulse/native" ] || exit 75
        ;;
    *)
        echo "Usage: $0 {start|stop|status|ready}"
        exit 1
        ;;
esac
