#!/bin/bash
set -e
source script/env.sh

echo ">>> Building Linux Kernel..."

if [ ! -f "$BUILD_DIR/bzImage" ]; then
    rm -rf $BUILD_DIR/linux
    tar -xf $SRC_DIR/linux-$LINUX_VER.tar.xz -C $BUILD_DIR
    mv $BUILD_DIR/linux-$LINUX_VER $BUILD_DIR/linux
    cd $BUILD_DIR/linux
    
    # Config
    if [ -f "$WORKSPACE/src/config/.config.linux" ]; then
        cp "$WORKSPACE/src/config/.config.linux" .config
    else
        make ARCH=x86_64 defconfig
    fi
    
    # Ensure EFI Support is enabled (for both cases)
    ./scripts/config --enable CONFIG_EFI
    ./scripts/config --enable CONFIG_EFI_STUB
    ./scripts/config --enable CONFIG_EFI_MIXED
    ./scripts/config --enable CONFIG_RELOCATABLE
    
    # Enable Live Boot Support
    ./scripts/config --enable CONFIG_BLK_DEV_LOOP
    ./scripts/config --enable CONFIG_SQUASHFS
    ./scripts/config --enable CONFIG_SQUASHFS_XZ
    ./scripts/config --enable CONFIG_SQUASHFS_ZSTD
    ./scripts/config --enable CONFIG_OVERLAY_FS
    ./scripts/config --enable CONFIG_DEVTMPFS
    ./scripts/config --disable CONFIG_DEBUG_STACK_USAGE
    
    # Build
    export PATH=$CROSS_DIR/bin:$PATH
    yes "" | make ARCH=x86_64 oldconfig
    make ARCH=x86_64 CROSS_COMPILE=$TARGET-
    
    cp arch/x86/boot/bzImage $BUILD_DIR/bzImage
    cd ..
    rm -rf linux
else
    echo ">>> Kernel bzImage already built."
fi
