#!/bin/bash
set -e
source script/env.sh

echo ">>> Packaging KDOS..."

INITRAMFS_WORK=$BUILD_DIR/initramfs_work
ISO_DIR=$BUILD_DIR/iso

# -----------------------------------------------------------------------------
# 1. Prepare Minimal Initramfs
# -----------------------------------------------------------------------------
echo ">>> Creating minimal initramfs..."
rm -rf $INITRAMFS_WORK
mkdir -p $INITRAMFS_WORK/{bin,sbin,lib,etc,dev,proc,sys,mnt,tmp,run,var}
mkdir -p $INITRAMFS_WORK/usr/{bin,sbin,lib}

# Copy Toybox (and libraries if dynamic)
cp $SYSROOT/bin/toybox $INITRAMFS_WORK/bin/
if [ -d "$SYSROOT/lib" ]; then
    cp -d $SYSROOT/lib/* $INITRAMFS_WORK/lib/
fi
# Just in case some libs are in usr/lib (rare for base musl/gcc libs but possible)
# but we try to keep it minimal. If toybox fails, we might need more.

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

# -----------------------------------------------------------------------------
# 2. Prepare RootFS (SquashFS)
# -----------------------------------------------------------------------------
echo ">>> Compressing RootFS (this may take a while)..."
# We exclude the boot directory content from rootfs if we want, but keeping it is fine.
# mksquashfs handles files.
rm -f $BUILD_DIR/rootfs.squashfs
mksquashfs $SYSROOT $BUILD_DIR/rootfs.squashfs -comp xz -noappend

# -----------------------------------------------------------------------------
# 3. Prepare ISO Structure
# -----------------------------------------------------------------------------
echo ">>> Preparing ISO directory..."
rm -rf $ISO_DIR
mkdir -p $ISO_DIR/EFI/BOOT
mkdir -p $ISO_DIR/boot

# Install Components
cp $BUILD_DIR/bzImage $ISO_DIR/EFI/BOOT/bzImage
cp $BUILD_DIR/init.cpio $ISO_DIR/EFI/BOOT/init.cpio
cp $BUILD_DIR/rootfs.squashfs $ISO_DIR/rootfs.squashfs  # Placed at root of ISO
cp $BUILD_DIR/bootloaders/refind-$REFIND_VER/refind/refind_x64.efi $ISO_DIR/EFI/BOOT/bootx64.efi

# Configure rEFInd
# Configure rEFInd
if [ -f "$WORKSPACE/fs/boot/refind.conf" ]; then
    cp "$WORKSPACE/fs/boot/refind.conf" $ISO_DIR/EFI/BOOT/refind.conf
else
     echo "ERROR: refind.conf not found in fs/boot/refind.conf"
     exit 1
fi

# -----------------------------------------------------------------------------
# 4. Generate ISO
# -----------------------------------------------------------------------------
echo ">>> Generatng efiboot.img..."
dd if=/dev/zero of=$BUILD_DIR/efiboot.img bs=1k count=1572864
/sbin/mkfs.fat -F 32 -n "KDOS_EFI" $BUILD_DIR/efiboot.img
mcopy -i $BUILD_DIR/efiboot.img -s $ISO_DIR/EFI ::/

echo ">>> Generating kdos.iso..."
xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -volid "KDOS" \
    -eltorito-alt-boot \
    -e efiboot.img \
    -no-emul-boot \
    -isohybrid-gpt-basdat \
    -output $BUILD_DIR/kdos.iso \
    -graft-points \
    /=$ISO_DIR \
    /efiboot.img=$BUILD_DIR/efiboot.img

echo ">>> Build Complete!"
echo "    ISO: build/kdos.iso"
echo "    To run (QEMU UEFI): 'make run'"
