#!/bin/bash
. /etc/init.d/service_helper

NAME="tlp"
DAEMON="/usr/sbin/tlp"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # NOT supervised: tlp is not a daemon. It applies a set of /sys
        # settings and exits, so handing it to ksvc would make the supervisor
        # respawn it forever.
        #
        # `init start`, not `start`: the init verb is the boot one, and is the
        # only one that applies DEVICES_TO_{DIS,EN}ABLE_ON_STARTUP. USB
        # autosuspend is not applied here — TLP's udev rule sets it per
        # device as coldplug adds them.
        echo "[KDOS] Applying $NAME power policy..."
        "$DAEMON" init start
        ;;
    stop)
        # `init stop` is the shutdown half: it clears the saved profile and
        # re-enables Wi-Fi that a battery or connect rule turned off, so the
        # next boot starts with it on. `tlp start` in its place would
        # re-apply a profile to a machine that is about to lose power.
        [ -x "$DAEMON" ] && "$DAEMON" init stop
        ;;
    status)
        # The first line is the summary, and the STATUS cell of `service
        # list` shows that line; `service status tlp` prints the whole of
        # `tlp-stat -s` under it. The summary reads three of its `key = value`
        # lines and keeps the state word of the `tlp` one, which goes on with
        # the time of the last run. A tlp-stat that prints no `tlp` line, as
        # it does when it refuses a caller without root, has its own first
        # line as the summary instead, so the cell carries its error.
        if [ ! -x /usr/bin/tlp-stat ]; then
            echo "$NAME: not installed"
            exit 0
        fi
        _st=$(tlp-stat -s 2>&1)
        _state=
        _profile=
        _source=
        _first=
        while IFS= read -r _l; do
            [ -z "$_first" ] && [ -n "${_l// }" ] && _first="$_l"
            case "$_l" in
                *=*) ;;
                *) continue ;;
            esac
            _k="${_l%%=*}"
            _v="${_l#*=}"
            _k="${_k%"${_k##*[! ]}"}"
            _v="${_v# }"
            case "$_k" in
                tlp)            _state="${_v%%,*}" ;;
                "TLP profile")  _profile="$_v" ;;
                "Power source") _source="$_v" ;;
            esac
        done <<< "$_st"
        if [ -n "$_state" ]; then
            echo "$NAME: $_state, profile ${_profile:-unknown}, power source ${_source:-unknown}"
        else
            echo "$NAME: ${_first:-tlp-stat printed nothing}"
        fi
        printf '%s\n' "$_st"
        ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
