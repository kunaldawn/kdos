#!/bin/bash
set -e
source script/env.sh

echo ">>> Compressing RootFS (this may take a while)..."
# We exclude the boot directory content from rootfs if we want, but keeping it is fine.
# mksquashfs handles files.
rm -f $BUILD_DIR/rootfs.squashfs
mksquashfs $SYSROOT $BUILD_DIR/rootfs.squashfs -comp xz -noappend
