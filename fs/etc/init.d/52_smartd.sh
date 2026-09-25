#!/bin/bash
. /etc/init.d/service_helper

NAME="smartd"
DAEMON="/usr/sbin/smartd"

# THE DISK-HEALTH WATCHER. smartd polls every SATA, SAS and NVMe disk that
# answers SMART every thirty minutes and logs to the system log a failing
# health check, pending or uncorrectable sectors, new entries in the drive's
# error log and failed self-tests. /etc/smartd.conf is upstream's and ships
# as a bare DEVICESCAN; a `-m` or `-M exec` line there adds a route beyond the
# log.
#
# -n keeps it in the foreground, which is what `supervise` needs.
#
# THREE EXITS ARE FINAL. 17 is "no devices to monitor" — a virtual machine, or
# a machine whose disks do not answer SMART — and 2 and 6 are a smartd.conf it
# cannot parse or cannot read. Restarting changes none of them, and without
# `--final-exit` the supervisor would log each one every five seconds.

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise --final-exit 2 --final-exit 6 --final-exit 17 "$NAME" \
            "$DAEMON" -n
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
