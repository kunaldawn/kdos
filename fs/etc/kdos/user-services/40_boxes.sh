#!/bin/bash
. /etc/init.d/service_helper

# THE USER'S BOXES STOP WITH THE USER'S SERVICES, at the last logout, unless
# the user lingers (`loginctl enable-linger`, which turnstile reads). Nothing
# is started here: a box is started by the login's warm-up and by its first
# launch. This script exists for its `stop`, which the turnstile backend runs
# first, because it runs the table backwards.
#
# EVERY CONTAINER OF THE USER'S, NOT ONLY KDOS's BOXES. A rootless container
# keeps its runtime state under $XDG_RUNTIME_DIR, and turnstile clears that
# directory once the last service has stopped: a container left running past
# that point has lost its sockets, its bus and its pasta network files, and
# the next login cannot even stop it. elogind's KillUserProcesses=no is what
# lets one survive a logout at all, so a lingering user's boxes keep running.
#
# Bounded, because the backend's whole stop has to fit turnstile's kill
# timeout (60 s), past which turnstiled signals the user's services again and
# then gives up on them: thirty seconds here — ten for the containers to take
# their SIGTERM, then podman's SIGKILL — the five-second grace of each of the
# other five services stopped after this one, and mpd's three.

NAME="boxes"

case "$1" in
    start|ready)
        exit 0
        ;;
    stop)
        command -v podman >/dev/null 2>&1 || exit 0
        _n=$(podman ps -q 2>/dev/null | grep -c .)
        [ "$_n" -gt 0 ] || exit 0
        echo "[KDOS] Stopping $NAME: $_n running container(s)"
        timeout 30 podman stop --all --time 10 >/dev/null
        ;;
    status)
        if ! command -v podman >/dev/null 2>&1; then
            echo "[DOWN] $NAME: podman not installed"
            exit 1
        fi
        echo "[ OK ] $NAME: $(podman ps -q 2>/dev/null | grep -c .) running"
        ;;
    *)
        echo "Usage: $0 {start|stop|status|ready}"
        exit 1
        ;;
esac
