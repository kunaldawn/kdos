#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/lsblk" ]; then
    exit 0
fi

echo ">>> Building Util-linux $UTIL_LINUX_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/util-linux-$UTIL_LINUX_VER.tar.xz
cd util-linux-$UTIL_LINUX_VER
mkdir -p $SYSROOT/var/lib/hwclock
./configure --host=$TARGET --prefix=/usr \
    --disable-chfn-chsh --disable-login --disable-nologin \
    --disable-su --disable-setpriv --disable-runuser \
    --disable-pylibmount --disable-static --without-python \
    --without-systemd --without-systemdsystemunitdir \
    --disable-liblastlog2 --disable-makeinstall-chown --disable-makeinstall-setuid
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf util-linux-$UTIL_LINUX_VER
