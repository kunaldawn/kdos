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

# Each --enable here only permits the probe: a missing header or library turns
# the feature off without an error, so readline, acl and zlib in depends are
# what keep xorriso -dialog line editing, ACLs and zisofs. libedit is only the
# fallback when readline is absent, and it is named off so that a readline
# failure cannot quietly swap the line editor. libjte and libcdio are not
# used: jigdo templates need a library that is not a port, and libcdio would
# replace libburn's own Linux SCSI adapter.
./configure --prefix=/usr \
	--enable-libreadline \
	--disable-libedit \
	--enable-libacl \
	--enable-zlib \
	--disable-libjte \
	--disable-libcdio \
	--enable-external-filters \
	--enable-launch-frontend
make
make -j1 DESTDIR=$PKG install

# xorriso takes its mkisofs personality from the name it is started under, as
# it does for the xorrisofs link upstream installs. K3b and growisofs run
# `mkisofs` from $PATH to build a data image, and a missing one is a data
# project that cannot be burned; genisoimage is the name other front ends try.
ln -s xorriso "$PKG/usr/bin/mkisofs"
ln -s xorriso "$PKG/usr/bin/genisoimage"
