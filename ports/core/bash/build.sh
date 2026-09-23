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

# Upstream's official patch series, in order. Each is a diff from the top of
# the release tree, so -p0 from $SRC.
for p in "$_pfx"-[0-9][0-9][0-9]; do
	patch -p0 -i "$p"
done

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
