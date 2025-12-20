#!/bin/bash
set -e
source script/env.sh

if [ -f "$CROSS_DIR/$TARGET/usr/include/unistd.h" ]; then
    exit 0
fi

echo ">>> Installing Musl Headers $MUSL_VER..."
mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

tar -xf $SRC_DIR/musl-$MUSL_VER.tar.gz
cd musl-$MUSL_VER
# We install headers to sysroot. Prefix /usr so they go to /usr/include
./configure --prefix=/usr --syslibdir=/lib
make install-headers DESTDIR=$SYSROOT
cd ..
rm -rf musl-$MUSL_VER
