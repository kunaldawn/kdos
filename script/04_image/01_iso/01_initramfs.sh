#!/bin/bash
set -e
source script/env.sh

INITRAMFS_WORK=$BUILD_DIR/initramfs_work

echo ">>> Creating minimal initramfs..."
rm -rf $INITRAMFS_WORK
mkdir -p $INITRAMFS_WORK/{bin,sbin,lib,etc,dev,proc,sys,mnt,tmp,run,var}
mkdir -p $INITRAMFS_WORK/usr/{bin,sbin,lib}

# Copy Toybox (and libraries if dynamic)
cp $SYSROOT/bin/toybox $INITRAMFS_WORK/bin/
if [ -d "$SYSROOT/lib" ]; then
    cp -d $SYSROOT/lib/* $INITRAMFS_WORK/lib/
fi

# Create Essential Symlinks for Toybox
cd $INITRAMFS_WORK/bin
for prog in sh mount umount mkdir sleep mknod echo cat ls switch_root ash grep wc awk sed cut find losetup; do
    ln -sf toybox $prog
done
# switch_root is often in sbin
cd $INITRAMFS_WORK/sbin
ln -sf ../bin/toybox switch_root
ln -sf ../bin/toybox init  # Toybox init if we fall back
cd $WORKSPACE

# Create Init Script
# Copy Init Script
if [ -f "$WORKSPACE/fs/usr/share/kdos/init" ]; then
    cp "$WORKSPACE/fs/usr/share/kdos/init" $INITRAMFS_WORK/init
else
    echo "ERROR: Init script not found in fs/usr/share/kdos/init"
    exit 1
fi

chmod +x $INITRAMFS_WORK/init

# Pack Initramfs
cd $INITRAMFS_WORK
find . -print0 | cpio --null -o -H newc --owner=0:0 > $BUILD_DIR/init.cpio
cd $WORKSPACE
