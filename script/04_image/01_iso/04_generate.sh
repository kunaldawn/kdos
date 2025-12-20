#!/bin/bash
set -e
source script/env.sh

ISO_DIR=$BUILD_DIR/iso

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
