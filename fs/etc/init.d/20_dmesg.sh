#!/bin/bash
. /etc/init.d/service_helper

NAME="dmesg"

case "$1" in
    start)
        # A boot without `quiet` (the verbose and single-user entries) is
        # one asked to show its messages: the console level stays the one
        # its `loglevel=` set. 22_syslog.sh keeps it there too.
        if ! grep -qw quiet /proc/cmdline 2>/dev/null; then
            echo "[KDOS] Verbose boot: console log level left as set"
            exit 0
        fi
        echo "[KDOS] Suppressing kernel messages on console..."
        dmesg -n 3
        ;;
    stop)
        # Restore default log level
        dmesg -n 7
        ;;
    status)
        echo "[ OK ] $NAME: console log level configured"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
