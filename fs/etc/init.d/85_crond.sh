#!/bin/bash
. /etc/init.d/service_helper

NAME="crond"
DAEMON="/usr/sbin/crond"
SPOOL="/var/spool/cron"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # USERS' CRONTABS, AND NOTHING THE SYSTEM SHIPS. Each user's own
        # crontab — what `crontab -e` and System Settings' Task Scheduler
        # write — runs from here. The timers the system ships are snooze's
        # (18_timers.sh, /etc/kdos/timers.d), and cronie is built to read no
        # /etc/crontab, so the two schedulers never share a job.
        #
        # The spool is root's and mode 0700: `crontab` is setuid and is the
        # only way a user's file gets in, after it has checked the file.
        # Made here when missing, because crond refuses to start without it.
        [ -d "$SPOOL" ] || install -d -m 0700 "$SPOOL"
        #
        # Late in the order, because an @reboot line runs the moment crond
        # starts and a user's job expects the machine it would find after a
        # login: the network, the printers, the servers above.
        #
        # -n: the foreground, for the supervisor. -s: a job's output goes to
        # syslog, which crond writes under its own cron facility — there is
        # no mail transport to deliver it, and -m off stops crond trying one.
        # -P: a job inherits this PATH, which rcS extends with /usr/local, so
        # a job naming `kdos` by its bare name finds it.
        supervise "$NAME" "$DAEMON" -n -s -m off -P
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
