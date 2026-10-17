#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

set -e
source script/phases/10_bootstrap/phase.env
source script/lib/port.sh

if [ -f "$MARK/patch" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building patch..."

PATCH_SRC=$(extract_port_source patch)

cd "$PATCH_SRC"

./configure \
    --host=$KDOS_TARGET \
    --build=$(build-aux/config.guess) \
    --prefix=/usr

make
make DESTDIR=$SYSROOT install

rm -rf "$PATCH_SRC"
touch "$MARK/patch"
