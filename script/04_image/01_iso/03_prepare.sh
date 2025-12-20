#!/bin/bash
set -e
source script/env.sh

ISO_DIR=$BUILD_DIR/iso

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
if [ -f "$WORKSPACE/fs/boot/refind.conf" ]; then
    cp "$WORKSPACE/fs/boot/refind.conf" $ISO_DIR/EFI/BOOT/refind.conf
else
     echo "ERROR: refind.conf not found in fs/boot/refind.conf"
     exit 1
fi
