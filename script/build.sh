#!/bin/bash
set -e

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
    make
    PREFIX=../fs make install
    cd ..
    rm -rf toybox

    # Build bash
    tar -xvf src/bash-5.3.tar.gz
    cd bash-5.3
    ./configure --prefix=/ --without-bash-malloc --enable-static-link
    make -j8
    make install DESTDIR=$(pwd)/../fs
    cd ..
    rm -rf bash-5.3

    # Build ncurses
    tar -xvf src/ncurses-6.5.tar.gz
    mv ncurses-6.5 ncurses
    cd ncurses
    ./configure --prefix=/ --without-shared --without-debug --enable-widec --without-ada --without-manpages --without-tests
    make -j8
    make install DESTDIR=$(pwd)/../fs
    cd ..
    rm -rf ncurses

    # Build nano
    tar -xvf src/nano-8.7.tar.xz
    mv nano-8.7 nano
    cd nano
    CPPFLAGS="-I$(pwd)/../fs/include -I$(pwd)/../fs/include/ncursesw" \
    LDFLAGS="-L$(pwd)/../fs/lib -static" \
    ./configure --prefix=/ --enable-utf8 --enable-tiny --disable-libmagic --disable-extra --disable-mouse --disable-help --disable-browser --disable-speller
    make -j8
    make install DESTDIR=$(pwd)/../fs
    cd ..
    rm -rf nano

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
