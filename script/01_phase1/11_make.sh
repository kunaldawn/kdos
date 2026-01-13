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

if [ -f "$MARK/make" ]; then
    exit 0
fi

echo ">>> Building make..."

# Extract make and dependencies from ports
MAKE_SRC=$(extract_port_source make)

cd "$MAKE_SRC"
mkdir -p build && cd build

../configure \
    --host=$KDOS_TARGET \
    --prefix=/usr \
    --disable-nls \
    --without-guile

make
make DESTDIR=$SYSROOT install

rm -rf "$MAKE_SRC"
touch "$MARK/make"
