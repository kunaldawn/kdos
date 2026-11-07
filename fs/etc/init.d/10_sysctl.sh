#!/bin/bash
. /etc/init.d/service_helper

# Kernel settings: every sysctl.d drop-in, then /etc/sysctl.conf.
#
# `--system` reads /etc/sysctl.d, /run/sysctl.d, /usr/local/lib/sysctl.d and
# /usr/lib/sysctl.d in file-name order, and /etc/sysctl.conf last. A program
# that raises a limit for good writes a drop-in — System Settings' inotify page
# writes /etc/sysctl.d/50-kde-inotify-survey-max_user_{watches,instances}.conf
# — and a boot that read /etc/sysctl.conf alone would lose it. A key in
# /etc/sysctl.conf wins over the same key in a drop-in, so sysctl.conf sets
# none of the limits a drop-in exists to raise.

NAME="sysctl"

case "$1" in
    start)
        echo "[KDOS] Applying sysctl settings..."
        sysctl --system
        ;;
    stop)
        # One-shot — nothing to stop
        ;;
    status)
        echo "[ OK ] $NAME: applied"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
