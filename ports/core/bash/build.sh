#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# SYS_BASHRC: make every interactive bash read /etc/bash.bashrc, so a
# terminal (non-login shell) gets the same aliases and prompt as a tty login.
export CFLAGS="$CFLAGS -DSYS_BASHRC='\"/etc/bash.bashrc\"'"

./configure --host=$TARGET \
	--prefix=/usr \
	--with-curses \
	--enable-readline \
	--without-bash-malloc \
	--with-installed-readline
make
make DESTDIR=$PKG install

mkdir -p $PKG/bin
mv $PKG/usr/bin/bash $PKG/bin
ln -sf bash $PKG/bin/sh
