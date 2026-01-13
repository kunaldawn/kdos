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
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/musl_libc" ]; then
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

