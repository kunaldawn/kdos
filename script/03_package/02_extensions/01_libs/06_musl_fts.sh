#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libfts.a" ]; then
    exit 0
fi

echo ">>> Building Musl-FTS $MUSL_FTS_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/musl-fts-$MUSL_FTS_VER.tar.gz
cd musl-fts-$MUSL_FTS_VER
./bootstrap.sh
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --disable-shared
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf musl-fts-$MUSL_FTS_VER

# Generate pkg-config file if missing (simple static linkage)
mkdir -p $SYSROOT/usr/lib/pkgconfig
cat > $SYSROOT/usr/lib/pkgconfig/musl-fts.pc <<EOF
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: musl-fts
Description: Implementation of fts(3) for musl libc
Version: $MUSL_FTS_VER
Libs: -L\${libdir} -lfts
Cflags: -I\${includedir}
EOF
