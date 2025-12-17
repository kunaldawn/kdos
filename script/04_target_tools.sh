#!/bin/bash
set -e
source script/env.sh

echo ">>> Building Target Tools..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar

# 1. Toybox
echo ">>> Building Toybox $TOYBOX_VER..."
    tar -xf $SRC_DIR/toybox-$TOYBOX_VER.tar.gz
    cd toybox-$TOYBOX_VER

    # Toybox expects 'cc'. Create symlink if missing.
    if [ ! -f "$CROSS_DIR/bin/$TARGET-cc" ]; then
        ln -sf $TARGET-gcc $CROSS_DIR/bin/$TARGET-cc
    fi

    # Unset ALL toolchain variables so Toybox uses CROSS_COMPILE + (cc|strip|etc)
    # This prevents double-prefixing (e.g. x86_64-kdos-...-x86_64-kdos-...-strip)
    unset CC CXX CFLAGS CXXFLAGS LDFLAGS AR AS LD RANLIB STRIP OBJCOPY
    export HOSTCC=gcc

    if [ -f "$WORKSPACE/src/config/.config.toybox" ]; then
        cp "$WORKSPACE/src/config/.config.toybox" .config
    else
        make -j1 defconfig
    fi

    # Clean previous install artifacts to avoid Permission Denied on stale files
    rm -f $SYSROOT/bin/toybox
    rm -f $SYSROOT/bin/toybox-x86_64-kdos-linux-musl

    PREFIX=$SYSROOT make -j1 install

    # Fix renaming if cross-compile suffix is used
    if [ -f "$SYSROOT/bin/toybox-x86_64-kdos-linux-musl" ]; then
        mv "$SYSROOT/bin/toybox-x86_64-kdos-linux-musl" "$SYSROOT/bin/toybox"
        # Create a link for the long name so that all other symlinks (mount, ls, etc) remain valid
        ln -sf toybox "$SYSROOT/bin/toybox-x86_64-kdos-linux-musl"
    fi

    # Restore env
    source $SCRIPT_DIR/env.sh

    cd ..
    rm -rf toybox-$TOYBOX_VER

    # Ensure it exists
    if [ ! -f "$SYSROOT/bin/toybox" ]; then
            echo "ERROR: Toybox install failed to create $SYSROOT/bin/toybox"
            exit 1
    fi

# 2. Bash
# Toybox may have created a symlink for bash (if configured to do so).
# We want the REAL bash, so remove the symlink if it exists.
if [ -L "$SYSROOT/bin/bash" ]; then
    rm -f "$SYSROOT/bin/bash"
fi

if [ ! -f "$SYSROOT/bin/bash" ]; then
    echo ">>> Building Bash $BASH_VER..."
    tar -xf $SRC_DIR/bash-$BASH_VER.tar.gz
    cd bash-$BASH_VER
    ./configure --host=$TARGET --prefix=/ --without-bash-malloc --enable-static-link
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf bash-$BASH_VER
    ln -sf bash "$SYSROOT/bin/sh"
fi

# 3. Dropbear
if [ ! -f "$SYSROOT/sbin/dropbear" ]; then
    echo ">>> Building Dropbear $DROPBEAR_VER..."
    tar -xf $SRC_DIR/dropbear-$DROPBEAR_VER.tar.bz2
    cd dropbear-$DROPBEAR_VER
    ./configure --host=$TARGET --prefix=/ --enable-zlib --enable-static
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf dropbear-$DROPBEAR_VER
fi

# 4. Curl
if [ ! -f "$SYSROOT/usr/bin/curl" ]; then
    echo ">>> Building Curl $CURL_VER..."
    tar -xf $SRC_DIR/curl-$CURL_VER.tar.gz
    cd curl-$CURL_VER
    ./configure --host=$TARGET --prefix=/usr --with-openssl --with-zlib \
        --disable-shared --enable-static \
        --without-libpsl --without-libidn2 \
        PKG_CONFIG_PATH="$SYSROOT/usr/lib/pkgconfig" \
        LDFLAGS="-static"
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf curl-$CURL_VER
fi

# 5. Socat
if [ ! -f "$SYSROOT/usr/bin/socat" ]; then
    echo ">>> Building Socat $SOCAT_VER..."
    tar -xf $SRC_DIR/socat-$SOCAT_VER.tar.gz
    cd socat-$SOCAT_VER
    ./configure --host=$TARGET --prefix=/usr
    # Fix strict C checks (GCC 15) and struct msghdr padding issues
    # Also user requested single-threaded build for this tool
    # IMPORTANT: Must include -D_GNU_SOURCE for sighandler_t in Musl
    make -j1 CFLAGS="-O2 -pipe --sysroot=$SYSROOT -D_GNU_SOURCE -Wno-int-conversion -Wno-error=int-conversion"
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf socat-$SOCAT_VER
fi

# 6. Tcpdump
if [ ! -f "$SYSROOT/usr/sbin/tcpdump" ]; then
    echo ">>> Building Tcpdump $TCPDUMP_VER..."
    tar -xf $SRC_DIR/tcpdump-$TCPDUMP_VER.tar.gz
    cd tcpdump-$TCPDUMP_VER
    ./configure --host=$TARGET --prefix=/usr
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf tcpdump-$TCPDUMP_VER
fi

# 7. Links
if [ ! -f "$SYSROOT/usr/bin/links" ]; then
    echo ">>> Building Links $LINKS_VER..."
    tar -xf $SRC_DIR/links-$LINKS_VER.tar.gz
    cd links-$LINKS_VER
    ./configure --host=$TARGET --prefix=/usr --with-ssl --enable-graphics=no
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf links-$LINKS_VER
fi

# 8. Htop
if [ ! -f "$SYSROOT/usr/bin/htop" ]; then
    echo ">>> Building Htop $HTOP_VER..."
    tar -xf $SRC_DIR/htop-$HTOP_VER.tar.xz
    cd htop-$HTOP_VER
    ./configure --host=$TARGET --prefix=/usr --enable-static --disable-unicode
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf htop-$HTOP_VER
fi

# 9. Bc
if [ ! -f "$SYSROOT/usr/bin/bc" ]; then
    echo ">>> Building Bc $BC_VER..."
    tar -xf $SRC_DIR/bc-$BC_VER.tar.gz
    cd bc-$BC_VER
    ./configure --host=$TARGET --prefix=/usr
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf bc-$BC_VER
fi

# 10. Jq
if [ ! -f "$SYSROOT/usr/bin/jq" ]; then
    echo ">>> Building Jq $JQ_VER..."
    tar -xf $SRC_DIR/jq-$JQ_VER.tar.gz
    cd jq-$JQ_VER
    ./configure --host=$TARGET --prefix=/usr --with-oniguruma=builtin --disable-maintainer-mode
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf jq-$JQ_VER
fi

# 11. Vim
if [ ! -f "$SYSROOT/usr/bin/vim" ]; then
    echo ">>> Building Vim $VIM_VER..."
    tar -xf $SRC_DIR/vim-$VIM_VER.tar.gz
    cd vim-$VIM_VER.0000
    export vim_cv_toupper_broken=no
    export vim_cv_terminfo=yes
    export vim_cv_tty_group=world
    export vim_cv_tty_mode=0620
    export vim_cv_getcwd_broken=no
    export vim_cv_stat_ignores_slash=no
    export vim_cv_memmove_handles_overlap=yes
    
    ./configure --host=$TARGET --prefix=/usr --with-features=normal \
        --enable-gui=no --without-x --disable-nls --disable-acl \
        --disable-gpm --disable-selinux 
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf vim-$VIM_VER.0000
fi

# 12. Tmux
if [ ! -f "$SYSROOT/usr/bin/tmux" ]; then
    echo ">>> Building Tmux $TMUX_VER..."
    tar -xf $SRC_DIR/tmux-$TMUX_VER.tar.gz
    cd tmux-$TMUX_VER
    ./configure --host=$TARGET --prefix=/usr --enable-static LIBEVENT_CFLAGS="-I$SYSROOT/usr/include" LIBEVENT_LIBS="-L$SYSROOT/usr/lib -levent"
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf tmux-$TMUX_VER
fi

# 13. Nano
if [ ! -f "$SYSROOT/bin/nano" ]; then
    echo ">>> Building Nano $NANO_VER..."
    tar -xf $SRC_DIR/nano-$NANO_VER.tar.xz
    cd nano-$NANO_VER
    ./configure --host=$TARGET --prefix=/ --enable-static --disable-shared --enable-utf8 \
        --enable-color --enable-nanorc --enable-multibuffer
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf nano-$NANO_VER
fi

# 14. Gawk
if [ ! -f "$SYSROOT/usr/bin/gawk" ]; then
    echo ">>> Building Gawk $GAWK_VER..."
    tar -xf $SRC_DIR/gawk-$GAWK_VER.tar.xz
    cd gawk-$GAWK_VER
    ./configure --host=$TARGET --prefix=/usr
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf gawk-$GAWK_VER
fi

# 15. Diffutils
if [ ! -f "$SYSROOT/usr/bin/diff" ]; then
    echo ">>> Building Diffutils $DIFFUTILS_VER..."
    tar -xf $SRC_DIR/diffutils-$DIFFUTILS_VER.tar.xz
    cd diffutils-$DIFFUTILS_VER
    ./configure --host=$TARGET --prefix=/usr
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf diffutils-$DIFFUTILS_VER
fi

# 16. Gzip
if [ ! -f "$SYSROOT/bin/gzip" ]; then
    echo ">>> Building Gzip $GZIP_VER..."
    tar -xf $SRC_DIR/gzip-$GZIP_VER.tar.xz
    cd gzip-$GZIP_VER
    ./configure --host=$TARGET --prefix=/
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf gzip-$GZIP_VER
fi

# 17. File
if [ ! -f "$SYSROOT/usr/bin/file" ]; then
    echo ">>> Building File $FILE_VER..."
    tar -xf $SRC_DIR/file-$FILE_VER.tar.gz
    cd file-$FILE_VER
    ./configure --host=$TARGET --prefix=/usr
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf file-$FILE_VER
fi

# 18. Util-linux
if [ ! -f "$SYSROOT/usr/bin/lsblk" ]; then
    echo ">>> Building Util-linux $UTIL_LINUX_VER..."
    tar -xf $SRC_DIR/util-linux-$UTIL_LINUX_VER.tar.xz
    cd util-linux-$UTIL_LINUX_VER
    mkdir -p $SYSROOT/var/lib/hwclock
    ./configure --host=$TARGET --prefix=/usr \
        --disable-chfn-chsh --disable-login --disable-nologin \
        --disable-su --disable-setpriv --disable-runuser \
        --disable-pylibmount --disable-static --without-python \
        --without-systemd --without-systemdsystemunitdir \
        --disable-liblastlog2 --disable-makeinstall-chown --disable-makeinstall-setuid
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf util-linux-$UTIL_LINUX_VER
fi

# 19. E2fsprogs
if [ ! -f "$SYSROOT/sbin/mkfs.ext4" ]; then
    echo ">>> Building E2fsprogs $E2FSPROGS_VER..."
    tar -xf $SRC_DIR/e2fsprogs-$E2FSPROGS_VER.tar.xz
    cd e2fsprogs-$E2FSPROGS_VER
    mkdir build && cd build
    ../configure --host=$TARGET --prefix=/usr --with-root-prefix="" --enable-elf-shlibs --disable-libblkid --disable-libuuid --disable-uuidd --disable-fsck
    make
    make install DESTDIR=$SYSROOT
    cd ../..
    rm -rf e2fsprogs-$E2FSPROGS_VER
fi

# 20. Dosfstools
if [ ! -f "$SYSROOT/sbin/mkfs.vfat" ]; then
    echo ">>> Building Dosfstools $DOSFSTOOLS_VER..."
    tar -xf $SRC_DIR/dosfstools-$DOSFSTOOLS_VER.tar.gz
    cd dosfstools-$DOSFSTOOLS_VER
    ./configure --host=$TARGET --prefix= --enable-compat-symlinks --mandir=/usr/share/man --docdir=/usr/share/doc/dosfstools-$DOSFSTOOLS_VER
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf dosfstools-$DOSFSTOOLS_VER
fi

# 21. Tree
if [ ! -f "$SYSROOT/usr/bin/tree" ]; then
    echo ">>> Building Tree $TREE_VER..."
    tar -xf $SRC_DIR/tree-$TREE_VER.tar.gz
    cd unix-tree-$TREE_VER
    # Tree typically has no configure script
    make CC=$TARGET-gcc
    make install PREFIX=$SYSROOT/usr
    cd ..
    rm -rf unix-tree-$TREE_VER
fi

# 22. Iproute2
if [ ! -f "$SYSROOT/sbin/ip" ]; then
    echo ">>> Building Iproute2 $IPROUTE2_VER..."
    tar -xf $SRC_DIR/iproute2-$IPROUTE2_VER.tar.xz
    cd iproute2-$IPROUTE2_VER
    # Point to kernel headers if needed, typically in $SYSROOT/usr/include
    # Musl fixes: -D_GNU_SOURCE
    make CC=$TARGET-gcc AR=$TARGET-ar CCOPTS="-O2 -pipe -I$SYSROOT/usr/include -D_GNU_SOURCE -DHAVE_SETNS -DHAVE_HANDLE_AT -include endian.h -include limits.h"
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf iproute2-$IPROUTE2_VER
fi

# 23. Wpa_supplicant
if [ ! -f "$SYSROOT/usr/sbin/wpa_supplicant" ]; then
    echo ">>> Building Wpa_supplicant $WPA_SUPPLICANT_VER..."
    tar -xf $SRC_DIR/wpa_supplicant-$WPA_SUPPLICANT_VER.tar.gz
    cd wpa_supplicant-$WPA_SUPPLICANT_VER/wpa_supplicant
    # Create .config by filtering out DBus and appending Libnl
    cp defconfig .config
    # Enable openssl and libnl, disable DBus
    sed -i 's/^CONFIG_CTRL_IFACE_DBUS=y/#CONFIG_CTRL_IFACE_DBUS=y/' .config
    sed -i 's/^CONFIG_CTRL_IFACE_DBUS_NEW=y/#CONFIG_CTRL_IFACE_DBUS_NEW=y/' .config
    sed -i 's/^CONFIG_CTRL_IFACE_DBUS_INTRO=y/#CONFIG_CTRL_IFACE_DBUS_INTRO=y/' .config
    sed -i 's/^#CONFIG_LIBNL32=y/CONFIG_LIBNL32=y/' .config

    make CC=$TARGET-gcc EXTRA_CFLAGS="-I$SYSROOT/usr/include -I$SYSROOT/usr/include/libnl3" \
        LIBS="-L$SYSROOT/usr/lib -lssl -lcrypto -lnl-3 -lnl-genl-3 -lnl-route-3" \
        BINDIR=/usr/sbin
    make install DESTDIR=$SYSROOT BINDIR=/usr/sbin
    cd ../..
    rm -rf wpa_supplicant-$WPA_SUPPLICANT_VER
fi

# 24. Python
if [ ! -f "$SYSROOT/usr/bin/python3" ]; then
    echo ">>> Building Python $PYTHON_VER..."
    tar -xf $SRC_DIR/Python-$PYTHON_VER.tar.xz
    cd Python-$PYTHON_VER
    
    # 1. Host Build (for cross-compilation tools)
    mkdir -p host-build
    cd host-build
    ../configure
    make
    cd ..

    # 2. Target Build (using host python)
    mkdir -p target-build
    cd target-build
    export ac_cv_file__dev_ptmx=yes
    export ac_cv_file__dev_ptc=no
    ../configure --host=$TARGET --build=x86_64-linux-gnu --prefix=/usr --disable-ipv6 --with-build-python=$(pwd)/../host-build/python
    make
    make install DESTDIR=$SYSROOT
    cd ../..
    rm -rf Python-$PYTHON_VER
fi

# 25. Git
if [ ! -f "$SYSROOT/usr/bin/git" ]; then
    echo ">>> Building Git $GIT_VER..."
    tar -xf $SRC_DIR/git-$GIT_VER.tar.xz
    cd git-$GIT_VER
    make configure
    ./configure --host=$TARGET --prefix=/usr --with-openssl --with-curl --with-zlib --without-tcltk
    make NO_GETTEXT=YesPlease NO_TCLTK=YesPlease
    make install DESTDIR=$SYSROOT NO_GETTEXT=YesPlease NO_TCLTK=YesPlease
    cd ..
    rm -rf git-$GIT_VER
fi

echo ">>> Target Tools Built."
