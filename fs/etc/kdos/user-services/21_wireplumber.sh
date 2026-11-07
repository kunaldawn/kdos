#!/bin/bash
. /etc/init.d/service_helper

# WIREPLUMBER IS THE SESSION MANAGER AND PIPEWIRE BUILDS NONE. The daemon
# routes nothing on its own: device discovery, which sink a stream lands on,
# and a Bluetooth headset's profile are all policy, and policy lives here.
# Without it there is a graph with nothing connected to anything — a machine
# with working hardware and no sound.
#
# STARTED AFTER pipewire AND BEFORE pipewire-pulse: it connects to the
# daemon's socket, and the PulseAudio shim announces sinks that do not exist
# until something has created them. `--restart-with pipewire` restarts it
# whenever the daemon it is connected to is restarted or stopped, because it
# does not exit when that connection closes.

NAME="wireplumber"
DAEMON="/usr/bin/wireplumber"

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
        exit 0
        ;;
    *)
        echo "Usage: $0 {start|stop|status|ready}"
        exit 1
        ;;
esac
