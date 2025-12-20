#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/git" ]; then
    exit 0
fi

echo ">>> Building Git $GIT_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/git-$GIT_VER.tar.xz
cd git-$GIT_VER
make configure
./configure --host=$TARGET --prefix=/usr --with-openssl --with-curl --with-zlib --without-tcltk
make NO_GETTEXT=YesPlease NO_TCLTK=YesPlease
make install DESTDIR=$SYSROOT NO_GETTEXT=YesPlease NO_TCLTK=YesPlease
cd ..
rm -rf git-$GIT_VER
