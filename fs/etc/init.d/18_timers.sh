#!/bin/bash
. /etc/init.d/service_helper

# ONE snooze PER JOB UNDER ksvc, and no scheduler of our own.
#
# `snooze` sleeps until its next matching time and runs one command; the
# supervisor restarts it, so it sleeps again. That is the whole loop, and it is
# why there is no cron daemon here: the thing a cron daemon adds over this is a
# scheduler, and KDOS already has a supervisor that does the same job with one
# process per timer and no shared state to corrupt.
#
# THE MISSED-RUN RULE IS snooze's `-s`, WRITTEN DOWN RATHER THAN REIMPLEMENTED.
# A machine asleep at the slot runs the job ONCE when it wakes, if it wakes
# within the slack. Not once per missed occurrence — a laptop shut for a
# fortnight would otherwise run fourteen catch-up jobs at breakfast.
#
# THIS STARTS EARLY BECAUSE A TIMER ONLY SLEEPS. Nothing it runs happens at
# boot unless the slack says a slot was missed, and a job that needs a daemon
# waits for its next slot like any other. Starting it late would mean a machine
# rebooted at 04:20 silently skipping a 04:17 job it was up for.

NAME="kdos-timers"
SYSDIR="/etc/kdos/timers.d"

# NAME TIMESPEC... -- COMMAND...
#
# Parsed, never sourced: the command is an argument vector and a line that
# does not parse is REPORTED and skipped. A timer that silently did not start
# is indistinguishable from one that has not fired yet, and the difference can
# be months.
timers_each() {   # <dir> <callback>
    _dir=$1
    _cb=$2
    [ -d "$_dir" ] || return 0
    for _f in "$_dir"/*.timer; do
        [ -e "$_f" ] || continue
        _line=0
        while IFS= read -r _row || [ -n "$_row" ]; do
            _line=$((_line + 1))
            case "$_row" in
                ''|'#'*) continue ;;
            esac
            # The name is the first word; everything up to `--` is snooze's.
            set -- $_row
            _name=$1
            shift
            _spec=""
            _seen=0
            while [ $# -gt 0 ]; do
                if [ "$1" = "--" ]; then
                    _seen=1
                    shift
                    break
                fi
                _spec="$_spec $1"
                shift
            done
            if [ "$_seen" != 1 ] || [ $# -eq 0 ]; then
                echo "[KDOS] ($NAME) $_f:$_line: no '--' or no command; skipped"
                continue
            fi
            case "$_name" in
                *[!a-zA-Z0-9_-]*|'')
                    echo "[KDOS] ($NAME) $_f:$_line: bad timer name '$_name'"
                    continue ;;
            esac
            "$_cb" "$_name" "$_spec" "$@"
        done < "$_f"
    done
}

timer_start() {   # <name> <spec> <command...>
    _n=$1; _s=$2; shift 2
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[SKIP] $NAME: $_n needs $1, which is not on this image"
        return 0
    fi
    # shellcheck disable=SC2086 — $_s IS a word list, which is the point.
    supervise "kdos-timer-$_n" snooze $_s "$@"
}

timer_stop() {    # <name> <spec> <command...>
    stop_service "kdos-timer-$1"
}

timer_status() {  # <name> <spec> <command...>
    check_status "kdos-timer-$1"
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
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
