#!/bin/bash
. /etc/init.d/service_helper

# THE USER'S PERIODIC JOBS, for as long as the user has a login — graphical,
# a tty, or the linger file that keeps the user's services up with none.
#
# One supervised daemon, /usr/local/lib/kdos/user-timers, runs every row of
# /etc/kdos/user-services/timers.d and ~/.config/kdos/timers.d as a loop of
# its own, in its process group: stopping this service ends every loop and
# every waiting snooze with the one group signal. The table's shape and its
# rules are /etc/kdos/timers.d's; the file that says what each row may hold is
# the README beside the user's table.
#
# `service --user restart timers` reads both tables again, and so does a
# SIGHUP to the pid in $XDG_RUNTIME_DIR/kdos/user-timers.pid; both stop a job
# that is running at that moment. A SIGUSR1 to the same pid starts only the
# files that have no loops yet, which is what `kdos remind` sends.

NAME="timers"
DAEMON="/usr/local/lib/kdos/user-timers"

case "$1" in
    start)
        if ! command -v snooze >/dev/null 2>&1; then
            echo "[SKIP] $NAME: snooze not found"
            exit 0
        fi
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON"
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    ready)
        # Once its pid file exists the runner has read both tables.
        check_status "$NAME" >/dev/null 2>&1 || exit 0
        [ -s "$XDG_RUNTIME_DIR/kdos/user-timers.pid" ] || exit 75
        ;;
    *)
        echo "Usage: $0 {start|stop|status|ready}"
        exit 1
        ;;
esac
