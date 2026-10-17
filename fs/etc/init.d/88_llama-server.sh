#!/bin/bash
. /etc/init.d/service_helper

# The local language-model server, an OpenAI-compatible API served to this
# machine alone at http://127.0.0.1:8082. 8080 is kiwix-serve's and 8081
# Kolibri's.
#
# Every GGUF model under /usr/share/llama.cpp/models — where a model pack
# installs — is offered, and the router loads one on the first request that
# names it, keeping one resident at a time: a second model loaded beside the
# first would need both in memory. With no model there is nothing to serve,
# and the service is skipped. It runs as nobody, since it only reads, with the
# render group so the Vulkan backend can open a GPU; with none it computes on
# the CPU.

NAME="llama-server"
DAEMON="/usr/bin/llama-server"
MODELS="/usr/share/llama.cpp/models"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if [ -z "$(find "$MODELS" -name '*.gguf' -print -quit 2>/dev/null)" ]; then
            echo "[SKIP] $NAME: no .gguf model under $MODELS"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=nobody --regid=nobody --groups=render \
            "$DAEMON" --host 127.0.0.1 --port 8082 \
            --models-dir "$MODELS" --models-max 1
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
