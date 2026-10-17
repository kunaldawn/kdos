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

for p in "$_pfx"-[0-9][0-9][0-9]; do
	patch -p0 -i "$p"
done

export CFLAGS="$CFLAGS -DSYS_BASHRC='\"/etc/bash.bashrc\"'"

./configure \
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
