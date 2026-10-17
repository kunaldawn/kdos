#!/bin/bash
. /etc/init.d/service_helper

# The offline library: every ZIM archive the library file lists, served to
# this machine alone at http://127.0.0.1:8080. kiwix-desktop reads the same
# archives on its own; this is the server the browser and the Caddy route in
# /etc/caddy/Caddyfile reach, and 8080 is the port the `kiwix` firewall name
# opens for someone who serves it on every address by hand.
#
# /var/lib/kiwix/library.xml is written by `kiwix-manage` when a data pack is
# installed or a library medium is mounted. With no library file there is
# nothing to serve, and the service is skipped rather than left failing:
# kiwix-serve exits at once when the library lists no archive. --monitorLibrary
# picks up an archive added later without a restart. It runs as nobody, since
# it only reads.

NAME="kiwix-serve"
DAEMON="/usr/bin/kiwix-serve"
LIBRARY="/var/lib/kiwix/library.xml"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if ! grep -q '<book ' "$LIBRARY" 2>/dev/null; then
            echo "[SKIP] $NAME: no archive listed in $LIBRARY (kiwix-manage $LIBRARY add <file.zim>)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=nobody --regid=nobody --clear-groups \
            "$DAEMON" --address=127.0.0.1 --port=8080 --monitorLibrary \
            --library "$LIBRARY"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
