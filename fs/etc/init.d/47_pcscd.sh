#!/bin/bash
. /etc/init.d/service_helper

NAME="pcscd"
DAEMON="/usr/sbin/pcscd"
DEFAULTS="/etc/default/pcscd"

# /etc/default/pcscd is upstream's file and upstream's keys, PARSED, NEVER
# SOURCED: one KEY=value per line, one pair of surrounding quotes removed, and
# $HOSTNAME, upstream's own example, the only expansion. pcscd reads the
# PCSCLITE_* names from its environment, so they are exported to it; an
# unknown key is reported and skipped rather than exported, because a typo in
# a filter name would otherwise hide nothing and say nothing.
ARGS=()
read_defaults() {
    [ -r "$DEFAULTS" ] || return 0
    local row key val line=0
    while IFS= read -r row || [ -n "$row" ]; do
        line=$((line + 1))
        row=${row#"${row%%[![:space:]]*}"}
        case "$row" in
            ''|'#'*) continue ;;
        esac
        key=${row%%=*}
        val=${row#*=}
        if [ "$key" = "$row" ]; then
            echo "[KDOS] ($NAME) $DEFAULTS:$line: not KEY=value; skipped"
            continue
        fi
        case "$val" in
            \"*\") val=${val:1:${#val}-2} ;;
            \'*\') val=${val:1:${#val}-2} ;;
        esac
        val=${val//'$HOSTNAME'/$HOSTNAME}
        case "$key" in
            PCSCLITE_FILTER_IGNORE_READER_NAMES | \
            PCSCLITE_FILTER_EXTEND_READER_NAMES | \
            PCSCLITE_HP_DROPDIR)
                export "$key=$val"
                ;;
            PCSCD_ARGS)
                # A word list, split here on purpose; noglob so a `*` in it
                # stays a `*`.
                set -f
                # shellcheck disable=SC2206
                ARGS=($val)
                set +f
                ;;
            *)
                echo "[KDOS] ($NAME) $DEFAULTS:$line: unknown key '$key'; skipped"
                ;;
        esac
    done < "$DEFAULTS"
}

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        read_defaults
        # After polkitd: every client is authorised through polkit, and a
        # daemon up before it refuses them all. --foreground for supervision;
        # readers come and go through udev hotplug, so it runs for ever.
        install -d -m 755 /run/pcscd
        supervise "$NAME" "$DAEMON" --foreground "${ARGS[@]}"
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
