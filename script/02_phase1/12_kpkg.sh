#!/bin/bash
set -e
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/kpkg" ]; then
    exit 0
fi

cp $WORKSPACE/src/kpkg/kpkg $SYSROOT/usr/bin/kpkg
cp $WORKSPACE/src/kpkg/kpkgadd $SYSROOT/usr/bin/kpkgadd
cp $WORKSPACE/src/kpkg/kpkgbuild $SYSROOT/usr/bin/kpkgbuild
cp $WORKSPACE/src/kpkg/kpkgdel $SYSROOT/usr/bin/kpkgdel
cp $WORKSPACE/src/kpkg/kpkgdepends $SYSROOT/usr/bin/kpkgdepends
cp $WORKSPACE/src/kpkg/kpkg.conf $SYSROOT/etc/kpkg.conf

touch "$MARK/kpkg"
