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

if [ -f "$MARK/gcc" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
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

# The native gcc's t-linux64 patch: the runtime goes to /usr/lib, and
# /usr/lib64 is only a link to it.
patch -p1 --no-backup-if-mismatch -i "$(port_dir gcc)/t-linux64-lib.patch"

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
    --libexecdir=/usr/lib      \
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
    CFLAGS_FOR_TARGET="${CFLAGS/-std=gnu[0-9][0-9]/}" \
    CXXFLAGS_FOR_TARGET="$CXXFLAGS -nostdinc++" \
    LDFLAGS_FOR_TARGET=-L$PWD/$KDOS_TARGET/libgcc

make BUILD_INFO=no-info LIBGCC2_DEBUG_CFLAGS=-g0
make DESTDIR=$SYSROOT install BUILD_INFO=no-info LIBGCC2_DEBUG_CFLAGS=-g0

ln -sv gcc $SYSROOT/usr/bin/cc

# No libtool archive outlives this step. Each names this build directory in
# a -L and the runtime directory as an RPATH, and libtool copies both into
# every C++ library linked through it. kpkg drops the .la files of a package,
# but these are installed outside any package, so nothing later removes them.
# The gdb script goes with them: no package owns it either.
rm -f $SYSROOT/usr/lib{,64}/lib{stdc++{,exp,fs},supc++,itm}.la \
    $SYSROOT/usr/lib{,64}/libstdc++.so.*-gdb.py \
    $SYSROOT/usr/lib/gcc/$KDOS_TARGET/$GCC_VER/liblto_plugin.la

rm -rf "$GCC_SRC" "$GMP_SRC" "$MPFR_SRC" "$MPC_SRC"
touch "$MARK/gcc"
