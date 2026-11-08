#!/bin/bash
# timers.sh — the one parser for every timer table.
#
# Sourced, never executed: by /etc/init.d/18_timers.sh for /etc/kdos/timers.d
# and by /usr/local/lib/kdos/user-timers for /etc/kdos/user-services/timers.d
# and ~/.config/kdos/timers.d. Two parsers of one format is one of them being
# wrong the day a field is added.
#
#   timers_each <dir> <callback>     every <dir>/*.timer, in name order
#   timers_file <file> <callback>    the one file
#
# A table file holds one row per line:
#
#   NAME TIMESPEC... -- COMMAND...
#
# and the callback is called as `<callback> NAME " TIMESPEC..." COMMAND...`
# for each row that parses, with TIMER_FILE set to the file the row came from.
# The timespec arrives as one word list, to be expanded unquoted under
# `set -f`.
#
# Parsed, never sourced: the command is an argument vector and a line that
# does not parse is REPORTED and skipped. A timer that silently did not start
# is indistinguishable from one that has not fired yet, and the difference can
# be months. The report names $NAME, which the caller sets to its own service
# name.
#
# The callback's status is not collected here: a caller that wants every row's
# answer gathers it in a variable of its own, because the function returns
# whatever its last command did, which is the last row's or a skipped comment
# row's 0.
timers_each() {   # <dir> <callback>
    [ -d "$1" ] || return 0
    for _tf in "$1"/*.timer; do
        [ -e "$_tf" ] || continue
        timers_file "$_tf" "$2"
    done
}

timers_file() {   # <file> <callback>
    _f=$1
    _cb=$2
    [ -r "$_f" ] || return 0
    _line=0
    while IFS= read -r _row || [ -n "$_row" ]; do
        _line=$((_line + 1))
        case "$_row" in
            ''|'#'*) continue ;;
        esac
        # The name is the first word; everything up to `--` is snooze's.
        #
        # NOGLOB AROUND THE SPLIT. The row is split by leaving the expansion
        # unquoted, which is the point — but that also PATHNAME-EXPANDS it,
        # and `*` is snooze's own syntax for "every". `-M *` in a directory
        # holding two files becomes `-M a b`: a timer that runs at times
        # nobody asked for, silently.
        set -f
        # shellcheck disable=SC2086 # the split is the point
        set -- $_row
        set +f
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
        # shellcheck disable=SC2034 # read by the callback
        TIMER_FILE=$_f
        "$_cb" "$_name" "$_spec" "$@"
    done < "$_f"
}
