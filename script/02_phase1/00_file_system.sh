#!/bin/bash
set -e
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/fs" ]; then
    exit 0
fi


mkdir -pv $SYSROOT/{etc,var,tmp,root,home,run,dev,proc,sys}
mkdir -pv $SYSROOT/usr/{bin,lib,sbin,include,share,local}
chmod 1777 $SYSROOT/tmp

cd $SYSROOT

# merged /usr
ln -sv usr/bin bin
ln -sv usr/sbin sbin
ln -sv usr/lib lib
ln -sv usr/lib64 lib64

cd $WORKSPACE

cp -r $WORKSPACE/fs/* $SYSROOT/

touch "$MARK/fs"
