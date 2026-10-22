#!/bin/bash
. /etc/init.d/service_helper

NAME="syslogd"
DAEMON="/usr/sbin/syslogd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /var/log
        # Runs before the network (30_network) on purpose: a boot that fails
        # while bringing an interface up is exactly the one whose log you want.
        # -F keeps it in the foreground for ksvc; without it the supervisor
        # watches a process that has already forked away.
        # -P moves syslogd's own pid file off /run/syslogd.pid, which is the
        # supervisor's: written there, it replaces the supervisor's id, so
        # `service stop` signals the daemon alone and it is restarted, and
        # syslogd removes it on exit, so `status` reports the service down.
        mkdir -p /run/syslogd
        supervise "$NAME" "$DAEMON" -F -P /run/syslogd/syslogd.pid
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
