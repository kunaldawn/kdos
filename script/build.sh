#!/bin/bash

# Check if bzImage does not exist, build kernel
if [ ! -f build/bzImage ]; then
    echo "bzImage does not exist, building kernel"

    tar -xvf src/linux-6.18.1.tar.xz
    mv linux-6.18.1 linux
    cp src/config/.config.linux linux/.config
    cd linux
    make -j8
    cp arch/x86/boot/bzImage ../build/bzImage
    cd ..
    rm -rf linux
fi

# Check if initrd does not exist, build initrd
if [ ! -f build/init.cpio ]; then
    echo "initrd does not exist, building initrd"
    
    # Create fs directory
    mkdir -p fs

    # Build musl
    tar -xvf src/musl-1.2.5.tar.gz
    mv musl-1.2.5 musl
    cd musl
    ./configure --prefix=/ --syslibdir=/lib
    make -j8
    DESTDIR=../fs make install
    cd ..
    rm -rf musl

    # Build toybox
    tar -xvf src/toybox-0.8.13.tar.gz
    mv toybox-0.8.13 toybox
    cp src/config/.config.toybox toybox/.config
    cd toybox
    make -j8
    PREFIX=../fs make install
    cd ..
    rm -rf toybox

    # Create fs directory
    mkdir -p fs/dev
    mkdir -p fs/proc
    mkdir -p fs/sys
    mkdir -p fs/usr/share

    # Create initrd
    cd fs
    ln -s sbin/init init
    find . -print0 | cpio --null -o -H newc --owner=0:0 > ../build/init.cpio
fi
