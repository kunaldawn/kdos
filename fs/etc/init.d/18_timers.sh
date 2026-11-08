#!/bin/bash
. /etc/init.d/service_helper

# ONE snooze PER JOB UNDER ksvc, and no scheduler of our own.
#
# `snooze` sleeps until its next matching time and runs one command; the
# supervisor restarts it, so it sleeps again. That is the whole loop, and it is
# why no cron daemon runs what the system ships: the thing a cron daemon adds
# over this is a scheduler, and KDOS already has a supervisor that does the
# same job with one process per timer and no shared state to corrupt. Users'
# crontabs — `crontab -e` and System Settings' Task Scheduler — are crond's
# (85_crond.sh), which reads no /etc/crontab, so no job is in both.
#
# THE MISSED-RUN RULE IS snooze's `-s`, WRITTEN DOWN RATHER THAN REIMPLEMENTED.
# A machine asleep at the slot runs the job ONCE when it wakes, if it wakes
# within the slack. Not once per missed occurrence — a laptop shut for a
# fortnight would otherwise run fourteen catch-up jobs at breakfast.
#
# THIS STARTS EARLY BECAUSE A TIMER ONLY SLEEPS. Nothing it runs happens at
# boot, and a job that needs a daemon waits for its next slot like any other.
#
# AND A REBOOT ACROSS A SLOT IS MISSED UNLESS THE LINE SAYS `-t`. A snooze that
# has just started begins looking one second from now, so `-s` alone covers a
# machine that was ASLEEP and not one that was OFF: `-t FILE` is what makes it
# start looking from that file's modification time instead, with the slack
# covering the gap. Starting this step early is still right — it is what makes
# the window a machine can miss as small as the boot is.

NAME="kdos-timers"
SYSDIR="/etc/kdos/timers.d"

# NAME TIMESPEC... -- COMMAND..., parsed by the same parser the per-user
# tables' runner uses; see /usr/local/lib/kdos/timers.sh for the row rules.
. /usr/local/lib/kdos/timers.sh

timer_start() {   # <name> <spec> <command...>
    _n=$1; _s=$2; shift 2
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[SKIP] $NAME: $_n needs $1, which is not on this image"
        return 0
    fi
    # $_s IS a word list, which is the point, and it is expanded under
    # noglob for the reason the parser splits it under noglob: `-H *` would
    # otherwise reach snooze as the names of the files in rcS's directory.
    set -f
    # shellcheck disable=SC2086
    supervise "kdos-timer-$_n" snooze $_s "$@"
    set +f
}

timer_stop() {    # <name> <spec> <command...>
    stop_service "kdos-timer-$1"
}

# Any job down makes the whole status non-zero. timers_each returns whatever
# its last command did, which is the last job's answer or a skipped comment
# row's 0, so the result is collected here rather than taken from it.
timers_rc=0
timer_status() {  # <name> <spec> <command...>
    check_status "kdos-timer-$1" || timers_rc=1
}

case "$1" in
    start)
        if ! command -v snooze >/dev/null 2>&1; then
            echo "[SKIP] $NAME: snooze not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        timers_each "$SYSDIR" timer_start
        ;;
    stop)
        timers_each "$SYSDIR" timer_stop
        ;;
    status)
        timers_each "$SYSDIR" timer_status
        exit "$timers_rc"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
