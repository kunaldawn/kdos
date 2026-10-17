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

if [ -f "$MARK/xz" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building xz..."

# Extract xz and dependencies from ports
XZ_SRC=$(extract_port_source xz)

cd "$XZ_SRC"

mkdir -p build && cd build

../configure \
    --host=$KDOS_TARGET \
    --prefix=/usr \
    --disable-static

make
make DESTDIR=$SYSROOT install

# No libtool archive outlives this step: it is installed outside any
# package, so nothing later removes it.
rm -f $SYSROOT/usr/lib/liblzma.la

rm -rf "$XZ_SRC"
touch "$MARK/xz"

