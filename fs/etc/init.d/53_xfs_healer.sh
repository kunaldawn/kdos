#!/bin/bash
. /etc/init.d/service_helper

NAME="xfs_healer"
DAEMON="/usr/libexec/xfsprogs/xfs_healer"

# ONE SUPERVISED xfs_healer PER MOUNTED XFS FILESYSTEM. It listens to the
# kernel's XFS health monitor for that mount and, where the filesystem's
# `autofsck` property asks for it, has the kernel repair damaged metadata
# online; otherwise it logs what the kernel found. Upstream starts it from a
# systemd template unit per mount, which is what this replaces.
#
# The filesystems are the ones mounted when this runs — the root and every
# fstab line. An XFS stick mounted later by kdos-mountd has no healer.
#
# `--supported` is asked first, per mount: a kernel without the health monitor,
# or a filesystem whose `autofsck` property is `none`, makes the daemon exit 1
# at once, and the supervisor would restart it for ever. After that the daemon
# exits only when its filesystem goes away (0) or on an error (1); both are
# final, and only a crash is restarted.
#
# Each instance is `xfs_healer-N`, N its line in the mount table, and `stop`
# finds them by their pid files rather than by re-reading the mounts, which
# may have changed since.

instances() {
    for _p in /run/"$NAME"-*.pid; do
        [ -e "$_p" ] || continue
        _i=${_p##*/}
        echo "${_i%.pid}"
    done
}

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        _n=0
        _started=0
        # /proc/self/mounts escapes a space, tab, newline or backslash in a
        # path as octal; printf %b turns it back into the path.
        while read -r _dev _mnt _type _rest; do
            _n=$((_n + 1))
            [ "$_type" = xfs ] || continue
            _mnt=$(printf '%b' "$_mnt")
            if ! "$DAEMON" --supported "$_mnt" >/dev/null 2>&1; then
                echo "[SKIP] $NAME: $_mnt: no health monitor, or autofsck=none"
                continue
            fi
            echo "[KDOS] Starting $NAME for $_mnt..."
            supervise --final-exit 0 --final-exit 1 "$NAME-$_n" "$DAEMON" "$_mnt"
            _started=$((_started + 1))
        done < /proc/self/mounts
        [ "$_started" -gt 0 ] || echo "[SKIP] $NAME: no XFS filesystem to watch"
        ;;
    stop)
        for _s in $(instances); do
            stop_service "$_s"
        done
        ;;
    status)
        _any=$(instances)
        if [ -z "$_any" ]; then
            echo "$NAME: not watching any filesystem"
            exit 1
        fi
        _rc=0
        for _s in $_any; do
            check_status "$_s" || _rc=1
        done
        exit $_rc
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
