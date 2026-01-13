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
source script/toolchain.env.sh
source script/util/port.sh

if [ -f "$MARK/binutils" ]; then
    exit 0
fi

echo ">>> Building Binutils..."

# Extract from port directory
BINUTILS_SRC=$(extract_port_source binutils)
cd "$BINUTILS_SRC"

mkdir build && cd build
../configure \
    --target=$KDOS_TARGET \
    --with-sysroot=$SYSROOT \
    --prefix=$CROSS_SYSROOT \
    --disable-nls \
    --disable-werror \
    --disable-multilib \
    --enable-new-dtags \
    --enable-gprofng=no \
    --enable-default-hash-style=gnu
make
make install

rm -rf "$BINUTILS_SRC"
touch "$MARK/binutils"
