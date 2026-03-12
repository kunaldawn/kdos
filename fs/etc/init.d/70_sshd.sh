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
        # Generate host keys if they don't exist
        [ ! -f /etc/ssh/ssh_host_rsa_key ] && ssh-keygen -q -t rsa -N "" -f /etc/ssh/ssh_host_rsa_key
        [ ! -f /etc/ssh/ssh_host_ed25519_key ] && ssh-keygen -q -t ed25519 -N "" -f /etc/ssh/ssh_host_ed25519_key
        # -D = don't detach (foreground for supervision)
        supervise "$NAME" "$DAEMON -D"
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
