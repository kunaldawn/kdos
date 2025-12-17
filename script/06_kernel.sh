#!/bin/bash
set -e
source script/env.sh

echo ">>> Building Linux Kernel..."

if [ ! -f "$BUILD_DIR/bzImage" ]; then
    tar -xf $SRC_DIR/linux-$LINUX_VER.tar.xz -C $BUILD_DIR
    mv $BUILD_DIR/linux-$LINUX_VER $BUILD_DIR/linux
    cd $BUILD_DIR/linux
    
    # Config
    if [ -f "$WORKSPACE/src/config/.config.linux" ]; then
        cp "$WORKSPACE/src/config/.config.linux" .config
    else
        make ARCH=x86_64 defconfig
    fi
    
    # Build
    export PATH=$CROSS_DIR/bin:$PATH
    make ARCH=x86_64 CROSS_COMPILE=$TARGET-
    
    cp arch/x86/boot/bzImage $BUILD_DIR/bzImage
    cd ..
    rm -rf linux
else
    echo ">>> Kernel bzImage already built."
fi
