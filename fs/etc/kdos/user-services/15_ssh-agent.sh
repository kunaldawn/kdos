#!/bin/bash
. /etc/init.d/service_helper

# THE ACCOUNT'S ONE ssh-agent, AT $XDG_RUNTIME_DIR/ssh-agent.socket, from the
# user's first login to the last logout. Every login and everything the
# desktop starts shares it, so `ssh-add` asks for a passphrase once.
# /etc/profile.d/50-ssh-agent.sh, from openssh, exports SSH_AUTH_SOCK to that
# path once the socket exists and starts nothing itself: an agent forked from
# a login shell belongs to no supervisor, outlives the runtime directory
# turnstile removes at the last logout, and the next login starts another
# beside it.
#
# READY WAITS FOR THE SOCKET. The backend holds the login until it reports,
# so the login shell's profile finds the socket and exports the variable.
#
# A STALE SOCKET IS REMOVED ONLY WHILE NO AGENT IS SUPERVISED. ssh-agent
# refuses to bind over an existing path, and one left by an agent that was
# killed would otherwise stop every restart; a running agent's socket is
# left alone, and `supervise` then starts nothing.
#
# --stop-grace 1: ssh-agent exits on SIGTERM at once, and the backend's whole
# stop has to fit turnstile's sixty seconds (40_boxes.sh counts it).

NAME="ssh-agent"
DAEMON="/usr/bin/ssh-agent"
SOCK="$XDG_RUNTIME_DIR/ssh-agent.socket"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        if [ -z "$XDG_RUNTIME_DIR" ] || [ ! -d "$XDG_RUNTIME_DIR" ]; then
            echo "[FAIL] $NAME: no runtime directory; turnstile makes it at the first login"
            exit 1
        fi
        check_status "$NAME" >/dev/null 2>&1 || rm -f "$SOCK"
        echo "[KDOS] Starting $NAME..."
        # -D keeps it in the foreground for supervision.
        supervise --stop-grace 1 "$NAME" "$DAEMON" -D -a "$SOCK"
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    ready)
        # 0 once the socket exists, 75 while it does not yet; 0 as well when
        # there is no supervised agent to wait for.
        check_status "$NAME" >/dev/null 2>&1 || exit 0
        [ -S "$SOCK" ] || exit 75
        ;;
    *)
        echo "Usage: $0 {start|stop|status|ready}"
        exit 1
        ;;
esac
