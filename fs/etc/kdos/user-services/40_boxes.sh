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
# `kdos-box stop`, NOT `podman stop`. A box that shares the host's PID
# namespace and has no cgroup of its own — every store box — is one `podman
# stop` signals nothing in, and it would hold this stop for its whole timeout
# and leave the box `stopping`. kdos-box signals each container's init and the
# applications its launches started, all containers together.
#
# THEN PODMAN'S PAUSE PROCESS. A rootless podman keeps one process per user
# holding its user namespace open, named in $XDG_RUNTIME_DIR/libpod/tmp/
# pause.pid, and it outlives every container. Left running it is orphaned
# with that directory gone, and the next login starts another beside it. It
# is ended by its pid, and only while that pid's environment still carries
# podman's _PODMAN_PAUSE=1, so a pid the kernel has given to something else
# since is left alone.
#
# Bounded, because the backend's whole stop has to fit turnstile's kill
# timeout (60 s), past which turnstiled signals the user's services again and
# then gives up on them: thirty seconds here — ten for the containers to take
# their SIGTERM, then SIGKILL and three more — then the stop grace of each
# service stopped after this one: five seconds for five of them, mpd's three
# and ssh-agent's one.

NAME="boxes"

# Ends the pause process pause.pid names, when it is still podman's.
stop_pause() {
    local _pidf="$XDG_RUNTIME_DIR/libpod/tmp/pause.pid" _pid
    [ -r "$_pidf" ] || return 0
    _pid=$(< "$_pidf")
    [[ "$_pid" =~ ^[0-9]+$ ]] || return 0
    tr '\0' '\n' < "/proc/$_pid/environ" 2>/dev/null | grep -qx '_PODMAN_PAUSE=1' || return 0
    kill -TERM "$_pid" 2>/dev/null
}

case "$1" in
    start|ready)
        exit 0
        ;;
    stop)
        command -v podman >/dev/null 2>&1 || exit 0
        mapfile -t _boxes < <(podman ps --format '{{.Names}}' 2>/dev/null)
        if [ "${#_boxes[@]}" -gt 0 ]; then
            echo "[KDOS] Stopping $NAME: ${#_boxes[@]} running container(s)"
            if [ -x /usr/local/bin/kdos-box ]; then
                timeout 30 /usr/local/bin/kdos-box stop "${_boxes[@]}" >/dev/null
            else
                timeout 30 podman stop --all --time 10 >/dev/null
            fi
        fi
        stop_pause
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
