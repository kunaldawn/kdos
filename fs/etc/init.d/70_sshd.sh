#!/bin/bash
. /etc/init.d/service_helper

NAME="sshd"
DAEMON="/usr/sbin/sshd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/sshd
        # -A makes each default host key type this OpenSSH knows that is
        # missing and leaves an existing key alone. sshd offers only the types
        # it finds a key file for, so a type left ungenerated is one a client
        # that insists on it cannot connect with.
        ssh-keygen -A
        # -D = don't detach (foreground for supervision). PidFile=none:
        # the supervisor already knows the daemon's pid and keeps its own
        # in /run/kdos-svc.sshd.pid, so a pid file of sshd's is one more
        # file to go stale and nothing reads it.
        supervise "$NAME" "$DAEMON" -D -o PidFile=none
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
