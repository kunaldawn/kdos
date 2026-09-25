#!/bin/bash
. /etc/init.d/service_helper

NAME="mdmonitor"
DAEMON="/usr/sbin/mdadm"

# THE RAID WATCHER. The initramfs assembles md arrays and nothing else looks at
# them afterwards, so without this a member disk can fail and the array run
# degraded, silently, until a second disk fails with it. `--monitor --scan`
# watches every array in /proc/mdstat and every one that appears later, and
# `--syslog` sends each event — Fail, DegradedArray, SparesMissing — to the
# system log; a MAILADDR or PROGRAM line in /etc/mdadm.conf adds its own route.
# It runs in the foreground, which is what `supervise` needs: no --daemonise.
#
# ONLY WHEN AN ARRAY EXISTS. With none, mdadm reports "No array with
# redundancy detected" and exits 0 — and it does the same if every array it
# found is RAID0. That exit, and the 1 of a configuration it cannot use, are
# final: restarting either changes nothing and would log it every five
# seconds.

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        if ! grep -q '^md' /proc/mdstat 2>/dev/null; then
            echo "[SKIP] $NAME: no md array on this machine"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise --final-exit 0 --final-exit 1 "$NAME" \
            "$DAEMON" --monitor --scan --syslog
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
