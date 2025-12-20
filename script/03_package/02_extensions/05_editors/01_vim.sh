#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/vim" ]; then
    exit 0
fi

echo ">>> Building Vim $VIM_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

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
