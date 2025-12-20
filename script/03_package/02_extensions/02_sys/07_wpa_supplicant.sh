#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/sbin/wpa_supplicant" ]; then
    exit 0
fi

echo ">>> Building Wpa_supplicant $WPA_SUPPLICANT_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/wpa_supplicant-$WPA_SUPPLICANT_VER.tar.gz
cd wpa_supplicant-$WPA_SUPPLICANT_VER/wpa_supplicant
# Create .config by filtering out DBus and appending Libnl
cp defconfig .config
# Enable openssl and libnl, disable DBus
sed -i 's/^CONFIG_CTRL_IFACE_DBUS=y/#CONFIG_CTRL_IFACE_DBUS=y/' .config
sed -i 's/^CONFIG_CTRL_IFACE_DBUS_NEW=y/#CONFIG_CTRL_IFACE_DBUS_NEW=y/' .config
sed -i 's/^CONFIG_CTRL_IFACE_DBUS_INTRO=y/#CONFIG_CTRL_IFACE_DBUS_INTRO=y/' .config
sed -i 's/^#CONFIG_LIBNL32=y/CONFIG_LIBNL32=y/' .config

make CC=$TARGET-gcc EXTRA_CFLAGS="-I$SYSROOT/usr/include -I$SYSROOT/usr/include/libnl3" \
    LIBS="-L$SYSROOT/usr/lib -lssl -lcrypto -lnl-3 -lnl-genl-3 -lnl-route-3" \
    BINDIR=/usr/sbin
make install DESTDIR=$SYSROOT BINDIR=/usr/sbin
cd ../..
rm -rf wpa_supplicant-$WPA_SUPPLICANT_VER
