#!/bin/bash
set -e
source script/env.sh

echo ">>> Packaging Initramfs..."

cd $SYSROOT

# 1. Finalize /init
# Ensure the critical binary exists!
if [ ! -f "bin/toybox" ]; then
    echo "CRITICAL ERROR: bin/toybox is missing! Initramfs will panic."
    exit 1
fi

if [ ! -f "init" ]; then
    ln -s bin/toybox init
fi

# 2. Copy Source Tarballs (For Self-Sufficiency)
# This allows rebuilding the OS from within the OS.
mkdir -p src
echo ">>> Copying sources to /src (This increases image size)..."
cp $SRC_DIR/* src/ || true

# 3. Create Initramfs
echo ">>> Creating init.cpio..."
find . -print0 | cpio --null -o -H newc --owner=0:0 > $BUILD_DIR/init.cpio

echo ">>> Build Complete!"
echo "    Kernel: build/bzImage"
echo "    Initrd: build/init.cpio"
echo "    To run: 'make run'"
