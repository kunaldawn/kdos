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
	--enable-xattr \
	--enable-zlib \
	--disable-libjte \
	--disable-libcdio \
	--enable-external-filters \
	--enable-launch-frontend
make
make -j1 DESTDIR=$PKG install
