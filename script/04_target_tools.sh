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

echo ">>> Target Tools Built."
