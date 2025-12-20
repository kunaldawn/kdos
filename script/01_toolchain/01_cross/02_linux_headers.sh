#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/include/linux/version.h" ]; then
    exit 0
fi

echo ">>> Installing Linux Headers $LINUX_VER..."
mkdir -p $SYSROOT/usr/include
cd $BUILD_DIR/tmp

tar -xf $SRC_DIR/linux-$LINUX_VER.tar.xz
cd linux-$LINUX_VER
make ARCH=x86_64 headers_install INSTALL_HDR_PATH=$SYSROOT/usr
cd ..
rm -rf linux-$LINUX_VER
