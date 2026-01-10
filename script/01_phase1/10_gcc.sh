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
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/gcc" ]; then
    exit 0
fi

echo ">>> Building gcc..."

# Extract gcc and dependencies from ports
GCC_SRC=$(extract_port_source gcc)
GCC_VER=$(get_port_version gcc)
GMP_SRC=$(extract_port_source gmp)
MPFR_SRC=$(extract_port_source mpfr)
MPC_SRC=$(extract_port_source mpc)

cd "$GCC_SRC"

# Link dependencies into GCC source tree
ln -s "$GMP_SRC" gmp
ln -s "$MPFR_SRC" mpfr
ln -s "$MPC_SRC" mpc

mkdir -p build && cd build

../configure \
    --host=$KDOS_TARGET \
    --target=$KDOS_TARGET \
    --with-build-sysroot=$SYSROOT \
    --prefix=/usr \
    --enable-default-pie       \
    --enable-default-ssp       \
    --disable-nls              \
    --disable-multilib         \
    --disable-libatomic        \
    --disable-libgomp          \
    --disable-libquadmath      \
    --disable-libsanitizer     \
    --disable-libssp           \
    --disable-libvtv           \
    --enable-languages=c,c++   \
    LDFLAGS_FOR_TARGET=-L$PWD/$KDOS_TARGET/libgcc

make
make DESTDIR=$SYSROOT install

ln -sv gcc $SYSROOT/usr/bin/cc



rm -rf "$GCC_SRC" "$GMP_SRC" "$MPFR_SRC" "$MPC_SRC"
touch "$MARK/gcc"
