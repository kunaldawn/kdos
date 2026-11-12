#!/bin/bash
. /etc/init.d/service_helper

# THE MUSIC DAEMON, ONCE SOMEBODY HAS CONFIGURED ONE. mpd exits at once with
# no configuration file and the image ships none, so a file in one of the
# places mpd itself reads is the opt-in; with none, this starts nothing and
# says so. It runs for the user's services' lifetime — from the first login
# to the last logout — so music started in one login plays on across a Log
# Out to a console login. Plasma's media controls reach it over MPRIS through
# `kdos-mpctl mpris`, which its own autostart entry starts in each Plasma
# session.
#
# AFTER pipewire-pulse (22): mpd's outputs go to PipeWire, and an output
# opened before the daemon answers is closed until the next play. It opens
# them when it plays, not when it starts, so it needs no --restart-with.
#
# $XDG_RUNTIME_DIR/mpd IS MADE HERE. kdos-mpctl and every client MPD_HOST
# points at (/etc/profile.d/45-mpd.sh) reach mpd through
# $XDG_RUNTIME_DIR/mpd/socket alone, and mpd creates that directory only when
# its configuration names no bind_to_address: a configuration that binds the
# socket there fails "No such file or directory" without it.
#
# --final-exit 1 is a configuration mpd refuses, which restarting every five
# seconds cannot change; --stop-grace 3 keeps the backend's whole stop inside
# turnstile's sixty seconds (40_boxes.sh counts it).

NAME="mpd"
DAEMON="/usr/bin/mpd"

config() {
    local _c
    for _c in "${XDG_CONFIG_HOME:-$HOME/.config}/mpd/mpd.conf" "$HOME/.mpdconf" \
              "$HOME/.mpd/mpd.conf" /etc/mpd.conf; do
        [ -r "$_c" ] && { printf '%s\n' "$_c"; return 0; }
    done
    return 1
}

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        if ! _conf=$(config); then
            echo "[SKIP] $NAME: no mpd.conf"
            exit 0
        fi
        install -d -m 0700 "$XDG_RUNTIME_DIR/mpd"
        echo "[KDOS] Starting $NAME..."
        supervise --final-exit 1 --stop-grace 3 "$NAME" "$DAEMON" --no-daemon "$_conf"
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    ready)
        exit 0
        ;;
    *)
        echo "Usage: $0 {start|stop|status|ready}"
        exit 1
        ;;
esac
