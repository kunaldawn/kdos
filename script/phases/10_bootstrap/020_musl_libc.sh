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

if [ -f "$MARK/musl_libc" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building Musl Libc..."

MUSL_SRC=$(extract_port_source musl)
cd "$MUSL_SRC"

./configure \
    CROSS_COMPILE=$KDOS_TARGET- \
    --prefix=/usr \
    --syslibdir=/lib
make
make DESTDIR=$SYSROOT install

rm -rf "$MUSL_SRC"
touch "$MARK/musl_libc"

