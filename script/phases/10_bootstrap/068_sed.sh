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

if [ -f "$MARK/sed" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building sed..."

SED_SRC=$(extract_port_source sed)

cd "$SED_SRC"

./configure \
    --host=$KDOS_TARGET \
    --build=$(build-aux/config.guess) \
    --prefix=/usr \
    --disable-nls

make
make DESTDIR=$SYSROOT install

rm -rf "$SED_SRC"
touch "$MARK/sed"
