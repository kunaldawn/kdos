#!/bin/bash
. /etc/init.d/service_helper

# THE MEDIA GRAPH, ONCE PER USER AND NOT ONCE PER SESSION. Every login of the
# user, graphical or not, and every box shares the sockets under
# $XDG_RUNTIME_DIR, so a second pipewire would fight the first over the same
# devices and the loser's clients would get silence; one supervisor per name is
# what makes a second impossible.
#
# THE REAL-TIME BUDGET COMES FROM PAM. The turnstile backend runs inside the
# `turnstiled` PAM session, whose pam_limits applies
# /etc/security/limits.d/25-pw-rlimits.conf (pipewire's, for the audio group):
# rtprio 95, nice -19, memlock 4 MiB. Without the group the daemon runs at
# normal priority and an underrun on a busy machine is a click.
#
# WIREPLUMBER (21) AND pipewire-pulse (22) FOLLOW IT. Both connect to this
# daemon's socket and neither exits when it goes away, so each is supervised
# `--restart-with pipewire`: a crash of this one restarts both once it is back.

NAME="pipewire"
DAEMON="/usr/bin/pipewire"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON"
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    ready)
        # WirePlumber connects at once and exits when there is no socket yet,
        # which would cost it a five-second respawn delay at every login.
        check_status "$NAME" >/dev/null 2>&1 || exit 0
        [ -S "$XDG_RUNTIME_DIR/pipewire-0" ] || exit 75
        ;;
    *)
        echo "Usage: $0 {start|stop|status|ready}"
        exit 1
        ;;
esac
