#!/bin/bash
# Load kernel modules listed in /etc/modules-load.d/*.conf.
# Runs after udev so module aliases are resolvable.

CONF_DIR="/etc/modules-load.d"

[ -d "$CONF_DIR" ] || exit 0

case "$1" in
    start)
        echo "[KDOS] Loading kernel modules from $CONF_DIR..."
        for f in "$CONF_DIR"/*.conf; do
            [ -r "$f" ] || continue
            while IFS= read -r mod; do
                # strip comments and whitespace
                mod="${mod%%#*}"
                mod="${mod//[[:space:]]/}"
                [ -z "$mod" ] && continue
                modprobe "$mod" 2>/dev/null && \
                    echo "[KDOS]   loaded: $mod" || \
                    echo "[KDOS]   skip:   $mod (no matching hardware)"
            done < "$f"
        done
        ;;
    stop|status)
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
