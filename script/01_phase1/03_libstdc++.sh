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

if [ -f "$MARK/libstdc++" ]; then
    exit 0
fi

echo ">>> Building libstdc++..."

# Extract GCC and dependencies from ports
GCC_SRC=$(extract_port_source gcc)
GCC_VER=$(get_port_version gcc)

cd "$GCC_SRC"

mkdir -p build && cd build

../libstdc++-v3/configure           \
    --host=$KDOS_TARGET                 \
    --build=$(../config.guess)      \
    --prefix=/usr                   \
    --disable-multilib               \
    --disable-nls                   \
    --disable-libstdcxx-pch         \
    --with-gxx-include-dir=/../cross/$KDOS_TARGET/include/c++/$GCC_VER

make DESTDIR=$SYSROOT install

# Remove the libtool archive files because they are harmful for cross-compilation
rm -v $SYSROOT/usr/lib64/lib{stdc++{,exp,fs},supc++}.la

rm -rf "$GCC_SRC"
touch "$MARK/libstdc++"

