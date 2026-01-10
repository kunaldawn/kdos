#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#    KDOS – forged by hand.
#    KD's Homebrew OS
# ---------------------------------

set -e
source script/toolchain.env.sh
source script/util/port.sh

if [ -f "$MARK/gcc" ]; then
    exit 0
fi

echo ">>> Building GCC..."

# Extract GCC and dependencies from ports
GCC_SRC=$(extract_port_source gcc)
GMP_SRC=$(extract_port_source gmp)
MPFR_SRC=$(extract_port_source mpfr)
MPC_SRC=$(extract_port_source mpc)

cd "$GCC_SRC"

# Link dependencies into GCC source tree
ln -s "$GMP_SRC" gmp
ln -s "$MPFR_SRC" mpfr
ln -s "$MPC_SRC" mpc

mkdir build && cd build

../configure \
    --target=$KDOS_TARGET \
    --with-sysroot=$SYSROOT \
    --prefix=$CROSS_SYSROOT \
    --without-headers \
    --with-newlib \
    --enable-languages=c,c++ \
    --enable-default-pie \
    --enable-default-ssp \
    --enable-initfini-array \
    --disable-nls \
    --disable-shared \
    --disable-multilib \
    --disable-threads \
    --disable-libatomic \
    --disable-libgomp \
    --disable-libquadmath \
    --disable-libssp \
    --disable-libvtv \
    --disable-libstdcxx \
    --disable-bootstrap

make all
make install

cd ..
cat gcc/limitx.h gcc/glimits.h gcc/limity.h > \
  `dirname $($KDOS_TARGET-gcc -print-libgcc-file-name)`/include/limits.h

ln -sv "$CROSS_SYSROOT/bin/$KDOS_TARGET-gcc" "$CROSS_SYSROOT/bin/$KDOS_TARGET-cc"

rm -rf "$GCC_SRC" "$GMP_SRC" "$MPFR_SRC" "$MPC_SRC"
touch "$MARK/gcc"
