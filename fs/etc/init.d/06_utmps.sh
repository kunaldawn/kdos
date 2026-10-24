#!/bin/bash
. /etc/init.d/service_helper

NAME="utmps"
UTMPD="/usr/bin/utmps-utmpd"
WTMPD="/usr/bin/utmps-wtmpd"
IPCSERVER="/usr/bin/s6-ipcserver"

case "$1" in
    start)
        if [ ! -x "$UTMPD" ] || [ ! -x "$IPCSERVER" ]; then
            echo "[SKIP] $NAME: utmps-utmpd or s6-ipcserver not found"
            exit 0
        fi
        echo "[KDOS] Starting utmpd and wtmpd..."
        # musl's utmp calls are stubs; every program that reads or writes a
        # login record goes through these two sockets instead. Both daemons
        # run as `utmp`, so the directory holding the live utmp file and the
        # wtmp file they append to belong to it.
        install -d -o utmp -g utmp -m 0755 /run/utmps
        # utmpd makes the utmp file only on its first request, and init
        # writes a record only to a utmp file that already exists: it checks
        # access() first and skips silently. Left to utmpd, the file appears
        # at the first getty's login record, after init has skipped the boot
        # and runlevel records, and `who -r` prints nothing.
        [ -e /run/utmps/utmp ] || : > /run/utmps/utmp
        chown utmp:utmp /run/utmps/utmp
        chmod 0644 /run/utmps/utmp
        [ -e /var/log/wtmp ] || : > /var/log/wtmp
        chown utmp:utmp /var/log/wtmp
        chmod 0644 /var/log/wtmp
        # utmps-utmpd opens `utmp` in its working directory.
        supervise utmpd /usr/bin/execline-cd /run/utmps \
            /usr/bin/s6-setuidgid utmp \
            "$IPCSERVER" -- /run/utmps/.utmpd-socket "$UTMPD"
        supervise wtmpd /usr/bin/s6-setuidgid utmp \
            "$IPCSERVER" -- /run/utmps/.wtmpd-socket "$WTMPD" /var/log/wtmp
        # init writes the boot record as soon as rcS returns, and a record
        # sent before the socket exists is lost: wait for both, up to 2 s.
        for _ in $(seq 20); do
            [ -S /run/utmps/.utmpd-socket ] && [ -S /run/utmps/.wtmpd-socket ] && break
            sleep 0.1
        done
        ;;
    stop)
        stop_service wtmpd
        stop_service utmpd
        ;;
    status)
        check_status utmpd && check_status wtmpd
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
