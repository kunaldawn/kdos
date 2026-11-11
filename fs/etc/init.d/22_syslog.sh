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
        # -P keeps syslogd's own pid file in /run/syslogd/ beside its cache;
        # the supervisor's is /run/kdos-svc.syslogd.pid and is what `stop`
        # and `status` read.
        # -8 writes bytes 0x80-0x9F as they arrive. Without it syslogd
        # rewrites each as `M-^x`, and those bytes are the continuation bytes
        # of most UTF-8 punctuation, so one em dash leaves a lone lead byte
        # in the file and grep calls /var/log/messages binary. ASCII control
        # characters are still escaped as `^x`.
        # syslogd creates a missing log file 0644 and keeps the mode of an
        # existing one across rotation, so these three are made root-only
        # here. Left to syslogd, every account could read who ran sudo and
        # which logins failed (auth.log), and the kernel's lines that dmesg
        # refuses it (kern.log, and messages, which takes kern.* too).
        for _log in /var/log/auth.log /var/log/kern.log /var/log/messages; do
            [ -e "$_log" ] || : > "$_log"
            chmod 600 "$_log"
        done
        # -l on a boot without `quiet`: syslogd otherwise turns the kernel's
        # console logging off when it opens the kernel log, and a verbose
        # boot would go silent on the screen here. 20_dmesg.sh is the other
        # half of that rule.
        _keep=()
        grep -qw quiet /proc/cmdline 2>/dev/null || _keep=(-l)
        mkdir -p /run/syslogd
        supervise "$NAME" "$DAEMON" -F -8 "${_keep[@]}" -P /run/syslogd/syslogd.pid
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
